/**
 * @file frame.hpp
 * @brief Frame encoding/decoding utilities (internal)
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "v4link/protocol.hpp"

namespace v4
{
namespace link
{
namespace internal
{

/**
 * @brief Encode a frame with command and payload
 *
 * Generates a complete frame: [STX][LEN_L][LEN_H][CMD][DATA...][CRC8]
 *
 * @param cmd     Command code
 * @param data    Payload data (can be nullptr if len == 0)
 * @param len     Payload length in bytes
 * @param out     Output buffer for encoded frame
 * @return true on success, false if payload exceeds MAX_PAYLOAD_SIZE
 */
bool encode_frame(Command cmd, const uint8_t* data, size_t len,
                  std::vector<uint8_t>& out);

/**
 * @brief Encode an ACK/NAK response frame
 *
 * Generates a response frame: [STX][0x00][0x01][ERR_CODE][CRC8]
 *
 * @param err_code Error code to send
 * @param out      Output buffer for encoded frame
 */
void encode_ack(ErrorCode err_code, std::vector<uint8_t>& out);

/**
 * @brief Verify frame CRC
 *
 * Checks if the CRC8 at the end of the frame matches the calculated checksum.
 * The CRC is calculated over [LEN_L][LEN_H][CMD][DATA...].
 *
 * @param frame   Complete frame buffer
 * @param len     Total frame length (including STX and CRC8)
 * @return true if CRC is valid, false otherwise
 */
bool verify_frame_crc(const uint8_t* frame, size_t len);

}  // namespace internal
}  // namespace link
}  // namespace v4
