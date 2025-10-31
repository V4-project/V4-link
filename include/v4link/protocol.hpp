/**
 * @file protocol.hpp
 * @brief V4-link protocol definitions
 *
 * Minimal bytecode transfer protocol for V4 VM.
 * Designed for embedded systems with tight memory constraints.
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace v4
{
namespace link
{

/* ========================================================================= */
/* Frame format constants                                                    */
/* ========================================================================= */

/**
 * @brief Start of frame marker
 *
 * All frames must begin with this byte.
 */
constexpr uint8_t STX = 0xA5;

/**
 * @brief Maximum payload size in bytes
 *
 * Limits the size of DATA field in a single frame.
 * Can be adjusted based on target platform RAM constraints.
 */
constexpr size_t MAX_PAYLOAD_SIZE = 512;

/**
 * @brief CRC-8 polynomial
 *
 * Uses polynomial 0x07 (x^8 + x^2 + x + 1)
 */
constexpr uint8_t CRC8_POLY = 0x07;

/* ========================================================================= */
/* Frame structure                                                           */
/* ========================================================================= */

/**
 * Frame format:
 *
 * [STX][LEN_L][LEN_H][CMD][DATA...][CRC8]
 *
 * - STX:   1 byte  (0xA5, start of frame marker)
 * - LEN_L: 1 byte  (payload length low byte, little-endian)
 * - LEN_H: 1 byte  (payload length high byte, little-endian)
 * - CMD:   1 byte  (command code)
 * - DATA:  N bytes (payload, 0 <= N <= MAX_PAYLOAD_SIZE)
 * - CRC8:  1 byte  (checksum of [LEN_L][LEN_H][CMD][DATA...])
 *
 * Minimum frame size: 5 bytes (STX + LEN_L + LEN_H + CMD + CRC8)
 * Maximum frame size: 517 bytes (5 + 512)
 */

/* ========================================================================= */
/* Command codes                                                             */
/* ========================================================================= */

/**
 * @brief Command codes for V4-link protocol
 */
enum class Command : uint8_t
{
  /**
   * @brief Execute bytecode
   *
   * DATA contains raw V4 bytecode to be executed immediately.
   * VM registers the bytecode as an anonymous word and executes it.
   *
   * Response: ACK with ERR_OK (0x00) on success, or error code on failure
   */
  EXEC = 0x10,

  /**
   * @brief Ping command
   *
   * Used to verify connection and check if the device is responsive.
   * DATA field is ignored (typically empty).
   *
   * Response: ACK with ERR_OK (0x00)
   */
  PING = 0x20,

  /**
   * @brief Reset VM
   *
   * Performs a complete VM reset (stacks, dictionary, memory).
   * DATA field is ignored (typically empty).
   *
   * Response: ACK with ERR_OK (0x00) after reset completes
   */
  RESET = 0xFF,
};

/* ========================================================================= */
/* Response codes                                                            */
/* ========================================================================= */

/**
 * @brief Response error codes
 *
 * Sent back to host in ACK/NAK frames.
 * Defined via errors.def for consistency with V4 VM.
 */
enum class ErrorCode : uint8_t
{
#define ERR(name, val, msg) name = val,
#include "v4link/errors.def"
#undef ERR
};

/**
 * @brief Response frame format
 *
 * [STX][0x00][0x01][ERR_CODE][CRC8]
 *
 * Response frames always have a fixed length of 5 bytes:
 * - STX: 0xA5
 * - LEN: 0x0001 (1 byte payload, little-endian)
 * - ERR_CODE: 1 byte from ErrorCode enum
 * - CRC8: Checksum of [0x00][0x01][ERR_CODE]
 */

}  // namespace link
}  // namespace v4
