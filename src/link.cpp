/**
 * @file link.cpp
 * @brief V4-link main implementation
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include "v4link/link.hpp"

#include "frame.hpp"
#include "v4/vm_api.h"
#include "v4/vm.h"  // For Word struct definition

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
      send_ack(ErrorCode::ERROR);
      break;
  }
}

void Link::handle_cmd_exec()
{
  // Payload starts at index 4 (after STX, LEN_L, LEN_H, CMD)
  const uint8_t* bytecode = buffer_.data() + 4;
  const size_t bytecode_len = frame_len_;

  // Copy bytecode to persistent storage (VM stores pointer, doesn't copy)
  std::vector<uint8_t> bytecode_copy(bytecode, bytecode + bytecode_len);
  bytecode_storage_.push_back(std::move(bytecode_copy));
  const uint8_t* persistent_bytecode = bytecode_storage_.back().data();

  // Register bytecode as anonymous word
  const int wid =
      vm_register_word(vm_, nullptr, persistent_bytecode, static_cast<int>(bytecode_len));

  if (wid < 0)
  {
    // Registration failed, remove the bytecode copy
    bytecode_storage_.pop_back();
    send_ack(ErrorCode::VM_ERROR);
    return;
  }

  // Get word entry
  Word* entry = vm_get_word(vm_, wid);
  if (entry == nullptr)
  {
    send_ack(ErrorCode::VM_ERROR);
    return;
  }

  // Execute bytecode
  // Note: Execution may fail for word definitions (e.g., [DUP, MUL, RET])
  // which should only be registered, not executed. We ignore execution errors
  // and always return the word index, as the word was registered successfully.
  vm_exec(vm_, entry);

  // Always return word index, even if execution failed
  uint8_t response_data[3];
  response_data[0] = 1;  // WORD_COUNT = 1 (always 1 for single bytecode execution)
  response_data[1] = static_cast<uint8_t>(wid & 0xFF);         // WORD_IDX_L
  response_data[2] = static_cast<uint8_t>((wid >> 8) & 0xFF);  // WORD_IDX_H
  send_ack(ErrorCode::OK, response_data, sizeof(response_data));
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

  // Get word name
  const char* name = word->name ? word->name : "";
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

  // Get bytecode
  uint16_t code_len = word->code_len;
  response_data.push_back(static_cast<uint8_t>(code_len & 0xFF));
  response_data.push_back(static_cast<uint8_t>((code_len >> 8) & 0xFF));

  if (word->code && code_len > 0)
  {
    for (uint16_t i = 0; i < code_len; ++i)
    {
      response_data.push_back(word->code[i]);
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
