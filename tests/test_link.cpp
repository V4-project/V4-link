/**
 * @file test_link.cpp
 * @brief V4-link unit tests
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <cstring>
#include <vector>

#include "crc8.hpp"
#include "frame.hpp"
#include "v4/vm_api.h"
#include "v4link/link.hpp"
#include "v4link/protocol.hpp"

using namespace v4::link;

/* ========================================================================= */
/* CRC8 Tests                                                                */
/* ========================================================================= */

TEST_CASE("CRC8 calculation")
{
  SUBCASE("Empty data")
  {
    const uint8_t data[] = {};
    const uint8_t crc = internal::calc_crc8(data, 0);
    CHECK(crc == 0x00);
  }

  SUBCASE("Single byte")
  {
    const uint8_t data[] = {0x42};
    const uint8_t crc = internal::calc_crc8(data, 1);
    CHECK(crc != 0x00);  // Should produce non-zero CRC
  }

  SUBCASE("Known test vector")
  {
    // Test with "123456789" (standard CRC test string)
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const uint8_t crc = internal::calc_crc8(data, 9);
    // CRC-8 with poly 0x07, init 0x00 should produce 0xF4 for this string
    CHECK(crc == 0xF4);
  }

  SUBCASE("Different data produces different CRC")
  {
    const uint8_t data1[] = {0x01, 0x02, 0x03};
    const uint8_t data2[] = {0x01, 0x02, 0x04};
    const uint8_t crc1 = internal::calc_crc8(data1, 3);
    const uint8_t crc2 = internal::calc_crc8(data2, 3);
    CHECK(crc1 != crc2);
  }
}

/* ========================================================================= */
/* Frame Encoding/Decoding Tests                                            */
/* ========================================================================= */

TEST_CASE("Frame encoding")
{
  SUBCASE("Empty payload")
  {
    std::vector<uint8_t> frame;
    const bool success = internal::encode_frame(Command::PING, nullptr, 0, frame);

    REQUIRE(success);
    CHECK(frame.size() == 5);  // STX + LEN_L + LEN_H + CMD + CRC
    CHECK(frame[0] == STX);
    CHECK(frame[1] == 0x00);  // LEN_L
    CHECK(frame[2] == 0x00);  // LEN_H
    CHECK(frame[3] == static_cast<uint8_t>(Command::PING));
  }

  SUBCASE("Small payload")
  {
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    std::vector<uint8_t> frame;
    const bool success = internal::encode_frame(Command::EXEC, payload, 3, frame);

    REQUIRE(success);
    CHECK(frame.size() == 8);  // STX + LEN(2) + CMD + DATA(3) + CRC
    CHECK(frame[0] == STX);
    CHECK(frame[1] == 0x03);  // LEN_L
    CHECK(frame[2] == 0x00);  // LEN_H
    CHECK(frame[3] == static_cast<uint8_t>(Command::EXEC));
    CHECK(frame[4] == 0x10);
    CHECK(frame[5] == 0x20);
    CHECK(frame[6] == 0x30);
  }

  SUBCASE("Payload too large")
  {
    const size_t large_size = MAX_PAYLOAD_SIZE + 1;
    std::vector<uint8_t> large_payload(large_size, 0xAA);
    std::vector<uint8_t> frame;
    const bool success =
        internal::encode_frame(Command::EXEC, large_payload.data(), large_size, frame);

    CHECK_FALSE(success);
  }
}

TEST_CASE("ACK frame encoding")
{
  std::vector<uint8_t> frame;
  internal::encode_ack(ErrorCode::OK, frame);

  REQUIRE(frame.size() == 5);
  CHECK(frame[0] == STX);
  CHECK(frame[1] == 0x01);  // LEN_L (payload = 1 byte, little-endian)
  CHECK(frame[2] == 0x00);  // LEN_H
  CHECK(frame[3] == static_cast<uint8_t>(ErrorCode::OK));
  // frame[4] is CRC
}

TEST_CASE("Frame CRC verification")
{
  SUBCASE("Valid frame")
  {
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::PING, nullptr, 0, frame);

    const bool valid = internal::verify_frame_crc(frame.data(), frame.size());
    CHECK(valid);
  }

  SUBCASE("Corrupted frame")
  {
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::PING, nullptr, 0, frame);

    // Corrupt the CRC byte
    frame[frame.size() - 1] ^= 0xFF;

    const bool valid = internal::verify_frame_crc(frame.data(), frame.size());
    CHECK_FALSE(valid);
  }

  SUBCASE("Frame too short")
  {
    const uint8_t short_frame[] = {STX, 0x00, 0x00};
    const bool valid = internal::verify_frame_crc(short_frame, 3);
    CHECK_FALSE(valid);
  }
}

/* ========================================================================= */
/* Link Class Tests                                                          */
/* ========================================================================= */

TEST_CASE("Link basic functionality")
{
  // Create VM
  uint8_t vm_memory[1024] = {0};
  VmConfig cfg = {vm_memory, sizeof(vm_memory), nullptr, 0, nullptr};
  Vm* vm = vm_create(&cfg);
  REQUIRE(vm != nullptr);

  // Track UART output
  std::vector<uint8_t> uart_output;
  auto uart_write = [&uart_output](const uint8_t* data, size_t len)
  { uart_output.insert(uart_output.end(), data, data + len); };

  Link link(vm, uart_write);

  SUBCASE("PING command")
  {
    // Encode PING frame
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::PING, nullptr, 0, frame);

    // Feed frame to link byte by byte
    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should receive ACK with OK
    REQUIRE(uart_output.size() == 5);
    CHECK(uart_output[0] == STX);
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::OK));
  }

  SUBCASE("RESET command")
  {
    // Push some test data to stack
    vm_ds_push(vm, 42);
    CHECK(vm_ds_depth_public(vm) == 1);

    // Encode RESET frame
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::RESET, nullptr, 0, frame);

    // Feed frame to link
    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should receive ACK
    REQUIRE(uart_output.size() == 5);
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::OK));

    // Stack should be cleared
    CHECK(vm_ds_depth_public(vm) == 0);
  }

  SUBCASE("EXEC command with simple bytecode")
  {
    // Simple bytecode: LIT 42 RET (push 42 to stack, then return)
    // Op::LIT = 0x00, followed by 4-byte little-endian value
    // Op::RET = 0x51
    const uint8_t bytecode[] = {
        0x00,                    // LIT opcode
        42,   0x00, 0x00, 0x00,  // 42 in little-endian
        0x51                     // RET opcode
    };

    // Encode EXEC frame
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::EXEC, bytecode, sizeof(bytecode), frame);

    // Feed frame to link
    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should receive ACK with OK and word index
    // Response: [STX][0x04][0x00][ERR_OK][WORD_COUNT=1][WORD_IDX_L][WORD_IDX_H][CRC]
    REQUIRE(uart_output.size() == 8);
    CHECK(uart_output[0] == STX);
    CHECK(uart_output[1] == 0x04);  // LEN_L = 4 (ERR + WORD_COUNT + WORD_IDX)
    CHECK(uart_output[2] == 0x00);  // LEN_H
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::OK));
    CHECK(uart_output[4] == 1);  // WORD_COUNT = 1
    // uart_output[5], [6] are WORD_IDX (depends on VM state)
    // uart_output[7] is CRC

    // Check if value was pushed to stack
    CHECK(vm_ds_depth_public(vm) == 1);
    CHECK(vm_ds_peek_public(vm, 0) == 42);
  }

  SUBCASE("Invalid CRC")
  {
    // Create frame with bad CRC
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::PING, nullptr, 0, frame);
    frame[frame.size() - 1] ^= 0xFF;  // Corrupt CRC

    // Feed frame to link
    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should receive NAK with INVALID_FRAME
    REQUIRE(uart_output.size() == 5);
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::INVALID_FRAME));
  }

  SUBCASE("Buffer overflow protection")
  {
    // Create frame with payload larger than buffer
    const size_t large_size = MAX_PAYLOAD_SIZE + 1;
    std::vector<uint8_t> large_payload(large_size, 0xAA);
    std::vector<uint8_t> frame;

    // Manually construct oversized frame header
    frame.push_back(STX);
    frame.push_back(static_cast<uint8_t>(large_size & 0xFF));
    frame.push_back(static_cast<uint8_t>((large_size >> 8) & 0xFF));

    // Feed just the header
    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should receive NAK with BUFFER_FULL
    REQUIRE(uart_output.size() == 5);
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::BUFFER_FULL));
  }

  vm_destroy(vm);
}

TEST_CASE("Link state machine robustness")
{
  uint8_t vm_memory[1024] = {0};
  VmConfig cfg = {vm_memory, sizeof(vm_memory), nullptr, 0, nullptr};
  Vm* vm = vm_create(&cfg);
  REQUIRE(vm != nullptr);

  std::vector<uint8_t> uart_output;
  auto uart_write = [&uart_output](const uint8_t* data, size_t len)
  { uart_output.insert(uart_output.end(), data, data + len); };

  Link link(vm, uart_write);

  SUBCASE("Garbage before valid frame")
  {
    // Send some garbage bytes
    link.feed_byte(0xFF);
    link.feed_byte(0x12);
    link.feed_byte(0x34);

    // Now send valid PING frame
    std::vector<uint8_t> frame;
    internal::encode_frame(Command::PING, nullptr, 0, frame);

    uart_output.clear();
    for (const uint8_t byte : frame)
    {
      link.feed_byte(byte);
    }

    // Should still process frame correctly
    REQUIRE(uart_output.size() == 5);
    CHECK(uart_output[3] == static_cast<uint8_t>(ErrorCode::OK));
  }

  vm_destroy(vm);
}
