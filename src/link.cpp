/**
 * @file link.cpp
 * @brief V4-link main implementation
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include "v4link/link.hpp"

#include <string>

#include "frame.hpp"
#include "v4/errors.hpp"
#include "v4/vm_api.h"

namespace v4
{
namespace link
{

Link::Link(Vm* vm, UartWriteFn uart_write, void* user, size_t buffer_size)
    : vm_(vm),
      uart_write_(uart_write),
      user_context_(user),
      buffer_(),
      pos_(0),
      state_(State::WAIT_STX),
      frame_len_(0),
      cmd_(0)
{
  buffer_.reserve(buffer_size + 4);  // Reserve space for header + payload
}

void Link::feed_byte(uint8_t byte)
{
  switch (state_)
  {
    case State::WAIT_STX:
      if (byte == STX)
      {
        buffer_.clear();
        buffer_.push_back(byte);
        pos_ = 0;
        state_ = State::WAIT_LEN_L;
      }
      break;

    case State::WAIT_LEN_L:
      buffer_.push_back(byte);
      frame_len_ = byte;
      state_ = State::WAIT_LEN_H;
      break;

    case State::WAIT_LEN_H:
      buffer_.push_back(byte);
      frame_len_ |= (static_cast<uint16_t>(byte) << 8);

      // Check if payload exceeds buffer capacity
      if (frame_len_ > buffer_.capacity() - 4)
      {
        send_ack(ErrorCode::BUFFER_FULL);
        state_ = State::WAIT_STX;
        break;
      }

      state_ = State::WAIT_CMD;
      break;

    case State::WAIT_CMD:
      buffer_.push_back(byte);
      cmd_ = byte;
      pos_ = 0;

      if (frame_len_ == 0)
      {
        // No payload, go directly to CRC
        state_ = State::WAIT_CRC;
      }
      else
      {
        state_ = State::WAIT_DATA;
      }
      break;

    case State::WAIT_DATA:
      buffer_.push_back(byte);
      pos_++;

      if (pos_ >= frame_len_)
      {
        state_ = State::WAIT_CRC;
      }
      break;

    case State::WAIT_CRC:
      buffer_.push_back(byte);
      handle_frame();
      state_ = State::WAIT_STX;
      break;
  }
}

void Link::handle_frame()
{
  // Verify CRC
  if (!internal::verify_frame_crc(buffer_.data(), buffer_.size()))
  {
    send_ack(ErrorCode::INVALID_FRAME);
    return;
  }

  // Dispatch command
  const Command cmd = static_cast<Command>(cmd_);

  switch (cmd)
  {
    case Command::EXEC:
      handle_cmd_exec();
      break;

    case Command::PING:
      handle_cmd_ping();
      break;

    case Command::QUERY_STACK:
      handle_cmd_query_stack();
      break;

    case Command::QUERY_MEMORY:
      handle_cmd_query_memory();
      break;

    case Command::QUERY_WORD:
      handle_cmd_query_word();
      break;

    case Command::RESET:
      handle_cmd_reset();
      break;

    default:
      send_ack(ErrorCode::GENERAL_ERROR);
      break;
  }
}

void Link::handle_cmd_exec()
{
  // Payload starts at index 4 (after STX, LEN_L, LEN_H, CMD)
  const uint8_t* payload = buffer_.data() + 4;
  const size_t payload_len = frame_len_;

  // Check if payload is a .v4b format (starts with "V4BC" magic)
  if (payload_len >= 16 && payload[0] == 0x56 && payload[1] == 0x34 && payload[2] == 0x42 &&
      payload[3] == 0x43)
  {
    // Parse .v4b header
    uint8_t version_minor = payload[5];
    uint32_t code_size =
        payload[8] | (payload[9] << 8) | (payload[10] << 16) | (payload[11] << 24);
    uint32_t word_count = 0;

    if (version_minor >= 2)
    {
      word_count = payload[12] | (payload[13] << 8) | (payload[14] << 16) | (payload[15] << 24);
    }

    // Validate payload size
    if (16 + code_size > payload_len)
    {
      send_ack(ErrorCode::GENERAL_ERROR);
      return;
    }

    std::vector<int> word_indices;

    // Register word definitions first (v0.2+)
    if (word_count > 0)
    {
      const uint8_t* word_ptr = payload + 16 + code_size;
      const uint8_t* payload_end = payload + payload_len;

      for (uint32_t i = 0; i < word_count; i++)
      {
        // Check remaining space
        if (word_ptr + 1 > payload_end)
        {
          send_ack(ErrorCode::GENERAL_ERROR);
          return;
        }

        // Read name length
        uint8_t name_len = *word_ptr++;

        // Read name
        if (word_ptr + name_len + 4 > payload_end)
        {
          send_ack(ErrorCode::GENERAL_ERROR);
          return;
        }

        std::string word_name(reinterpret_cast<const char*>(word_ptr), name_len);
        word_ptr += name_len;

        // Read code length
        uint32_t word_code_len =
            word_ptr[0] | (word_ptr[1] << 8) | (word_ptr[2] << 16) | (word_ptr[3] << 24);
        word_ptr += 4;

        // Check code fits
        if (word_ptr + word_code_len > payload_end)
        {
          send_ack(ErrorCode::GENERAL_ERROR);
          return;
        }

        // Copy word code to persistent storage
        std::vector<uint8_t> word_code_copy(word_ptr, word_ptr + word_code_len);
        bytecode_storage_.push_back(std::move(word_code_copy));
        const uint8_t* persistent_word_code = bytecode_storage_.back().data();

        // Register word with VM
        const int wid =
            vm_register_word(vm_, word_name.c_str(), persistent_word_code, word_code_len);

        if (wid < 0)
        {
          bytecode_storage_.pop_back();
          send_ack(ErrorCode::VM_ERROR);
          return;
        }

        word_indices.push_back(wid);
        word_ptr += word_code_len;
      }
    }

    // Register and execute main bytecode
    const uint8_t* main_code = payload + 16;
    std::vector<uint8_t> main_code_copy(main_code, main_code + code_size);
    bytecode_storage_.push_back(std::move(main_code_copy));
    const uint8_t* persistent_main_code = bytecode_storage_.back().data();

    const int main_wid = vm_register_word(vm_, nullptr, persistent_main_code, code_size);

    if (main_wid < 0)
    {
      bytecode_storage_.pop_back();
      send_ack(ErrorCode::VM_ERROR);
      return;
    }

    word_indices.push_back(main_wid);

    // Execute main bytecode
    Word* entry = vm_get_word(vm_, main_wid);
    if (entry)
    {
      vm_exec(vm_, entry);
    }

    // Return all word indices
    std::vector<uint8_t> response_data;
    response_data.push_back(static_cast<uint8_t>(word_indices.size()));
    for (int wid : word_indices)
    {
      response_data.push_back(static_cast<uint8_t>(wid & 0xFF));
      response_data.push_back(static_cast<uint8_t>((wid >> 8) & 0xFF));
    }
    send_ack(ErrorCode::OK, response_data.data(), response_data.size());
  }
  else
  {
    // Legacy raw bytecode (no .v4b header)
    std::vector<uint8_t> bytecode_copy(payload, payload + payload_len);
    bytecode_storage_.push_back(std::move(bytecode_copy));
    const uint8_t* persistent_bytecode = bytecode_storage_.back().data();

    const int wid = vm_register_word(vm_, nullptr, persistent_bytecode, payload_len);

    if (wid < 0)
    {
      bytecode_storage_.pop_back();
      send_ack(ErrorCode::VM_ERROR);
      return;
    }

    Word* entry = vm_get_word(vm_, wid);
    if (entry)
    {
      vm_exec(vm_, entry);
    }

    uint8_t response_data[3];
    response_data[0] = 1;
    response_data[1] = static_cast<uint8_t>(wid & 0xFF);
    response_data[2] = static_cast<uint8_t>((wid >> 8) & 0xFF);
    send_ack(ErrorCode::OK, response_data, sizeof(response_data));
  }
}

void Link::handle_cmd_ping()
{
  send_ack(ErrorCode::OK);
}

void Link::handle_cmd_reset()
{
  vm_reset(vm_);
  bytecode_storage_.clear();  // Free all allocated bytecode
  send_ack(ErrorCode::OK);
}

void Link::handle_cmd_query_stack()
{
  // Response format: [ERR_CODE][DS_DEPTH][DS_VALUES...][RS_DEPTH][RS_VALUES...]
  std::vector<uint8_t> response_data;

  // Get data stack
  int ds_depth = vm_ds_depth_public(vm_);
  if (ds_depth < 0)
  {
    send_ack(ErrorCode::VM_ERROR);
    return;
  }

  response_data.push_back(static_cast<uint8_t>(ds_depth));

  // Copy data stack values (up to 256 values)
  if (ds_depth > 0)
  {
    v4_i32 ds_data[256];
    int ds_count = vm_ds_copy_to_array(vm_, ds_data, 256);
    for (int i = 0; i < ds_count; ++i)
    {
      // Little-endian i32
      response_data.push_back(static_cast<uint8_t>(ds_data[i] & 0xFF));
      response_data.push_back(static_cast<uint8_t>((ds_data[i] >> 8) & 0xFF));
      response_data.push_back(static_cast<uint8_t>((ds_data[i] >> 16) & 0xFF));
      response_data.push_back(static_cast<uint8_t>((ds_data[i] >> 24) & 0xFF));
    }
  }

  // Get return stack
  int rs_depth = vm_rs_depth_public(vm_);
  if (rs_depth < 0)
  {
    send_ack(ErrorCode::VM_ERROR);
    return;
  }

  response_data.push_back(static_cast<uint8_t>(rs_depth));

  // Copy return stack values (up to 64 values)
  if (rs_depth > 0)
  {
    v4_i32 rs_data[64];
    int rs_count = vm_rs_copy_to_array(vm_, rs_data, 64);
    for (int i = 0; i < rs_count; ++i)
    {
      // Little-endian i32
      response_data.push_back(static_cast<uint8_t>(rs_data[i] & 0xFF));
      response_data.push_back(static_cast<uint8_t>((rs_data[i] >> 8) & 0xFF));
      response_data.push_back(static_cast<uint8_t>((rs_data[i] >> 16) & 0xFF));
      response_data.push_back(static_cast<uint8_t>((rs_data[i] >> 24) & 0xFF));
    }
  }

  send_ack(ErrorCode::OK, response_data.data(), response_data.size());
}

void Link::handle_cmd_query_memory()
{
  // Request format: [ADDR (4 bytes)][LEN (2 bytes)]
  if (frame_len_ < 6)
  {
    send_ack(ErrorCode::INVALID_FRAME);
    return;
  }

  const uint8_t* payload = buffer_.data() + 4;

  // Parse address (little-endian u32)
  uint32_t addr =
      payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);

  // Parse length (little-endian u16, max 256)
  uint16_t len = payload[4] | (payload[5] << 8);
  if (len > 256)
  {
    len = 256;
  }

  // Response format: [ERR_CODE][DATA...]
  std::vector<uint8_t> response_data;

  // Read memory (assuming vm_mem_read32 is available)
  for (uint16_t i = 0; i < len; i += 4)
  {
    uint32_t offset = addr + i;
    v4_u32 value = 0;
    if (vm_mem_read32(vm_, offset, &value) != V4_OK)
    {
      // On error, return zeros
      value = 0;
    }

    // Add up to 4 bytes (handle partial read at end)
    for (int j = 0; j < 4 && (i + j) < len; ++j)
    {
      response_data.push_back(static_cast<uint8_t>((value >> (j * 8)) & 0xFF));
    }
  }

  send_ack(ErrorCode::OK, response_data.data(), response_data.size());
}

void Link::handle_cmd_query_word()
{
  // Request format: [WORD_IDX (2 bytes)]
  if (frame_len_ < 2)
  {
    send_ack(ErrorCode::INVALID_FRAME);
    return;
  }

  const uint8_t* payload = buffer_.data() + 4;

  // Parse word index (little-endian u16)
  uint16_t word_idx = payload[0] | (payload[1] << 8);

  // Get word from VM
  Word* word = vm_get_word(vm_, word_idx);
  if (!word)
  {
    send_ack(ErrorCode::VM_ERROR);
    return;
  }

  // Response format: [ERR_CODE][NAME_LEN][NAME...][CODE_LEN][CODE...]
  std::vector<uint8_t> response_data;

  // Get word name via API
  const char* name = vm_word_get_name(word);
  if (!name)
    name = "";

  size_t name_len = 0;
  while (name[name_len] != '\0' && name_len < 63)
  {
    ++name_len;
  }

  response_data.push_back(static_cast<uint8_t>(name_len));
  for (size_t i = 0; i < name_len; ++i)
  {
    response_data.push_back(static_cast<uint8_t>(name[i]));
  }

  // Get bytecode via API
  uint16_t code_len = vm_word_get_code_len(word);
  response_data.push_back(static_cast<uint8_t>(code_len & 0xFF));
  response_data.push_back(static_cast<uint8_t>((code_len >> 8) & 0xFF));

  const v4_u8* code = vm_word_get_code(word);
  if (code && code_len > 0)
  {
    for (uint16_t i = 0; i < code_len; ++i)
    {
      response_data.push_back(code[i]);
    }
  }

  send_ack(ErrorCode::OK, response_data.data(), response_data.size());
}

void Link::send_ack(ErrorCode code, const uint8_t* data, size_t data_len)
{
  std::vector<uint8_t> ack_frame;
  internal::encode_ack(code, ack_frame, data, data_len);
  uart_write_(user_context_, ack_frame.data(), ack_frame.size());
}

void Link::reset()
{
  vm_reset(vm_);
}

}  // namespace link
}  // namespace v4
