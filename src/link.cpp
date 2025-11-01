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

namespace v4
{
namespace link
{

Link::Link(Vm* vm, UartWriteFn uart_write, size_t buffer_size)
    : vm_(vm),
      uart_write_(uart_write),
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

  // Register bytecode as anonymous word
  const int wid =
      vm_register_word(vm_, nullptr, bytecode, static_cast<int>(bytecode_len));

  if (wid < 0)
  {
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
  const v4_err err = vm_exec(vm_, entry);

  if (err < 0)
  {
    send_ack(ErrorCode::VM_ERROR);
  }
  else
  {
    // Send success response with word index
    // Response payload: [WORD_COUNT][WORD_IDX_L][WORD_IDX_H]
    uint8_t response_data[3];
    response_data[0] = 1;  // WORD_COUNT = 1 (always 1 for single bytecode execution)
    response_data[1] = static_cast<uint8_t>(wid & 0xFF);         // WORD_IDX_L
    response_data[2] = static_cast<uint8_t>((wid >> 8) & 0xFF);  // WORD_IDX_H
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
  send_ack(ErrorCode::OK);
}

void Link::send_ack(ErrorCode code, const uint8_t* data, size_t data_len)
{
  std::vector<uint8_t> ack_frame;
  internal::encode_ack(code, ack_frame, data, data_len);
  uart_write_(ack_frame.data(), ack_frame.size());
}

void Link::reset()
{
  vm_reset(vm_);
}

}  // namespace link
}  // namespace v4
