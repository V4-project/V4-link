/**
 * @file frame.cpp
 * @brief Frame encoding/decoding implementation
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include "frame.hpp"

#include "crc8.hpp"

namespace v4
{
namespace link
{
namespace internal
{

bool encode_frame(Command cmd, const uint8_t* data, size_t len, std::vector<uint8_t>& out)
{
  // Check payload size limit
  if (len > MAX_PAYLOAD_SIZE)
  {
    return false;
  }

  // Calculate total frame size: STX(1) + LEN(2) + CMD(1) + DATA(len) + CRC(1)
  const size_t frame_size = 5 + len;
  out.clear();
  out.reserve(frame_size);

  // Frame header
  out.push_back(STX);
  out.push_back(static_cast<uint8_t>(len & 0xFF));         // LEN_L
  out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));  // LEN_H
  out.push_back(static_cast<uint8_t>(cmd));

  // Payload
  if (len > 0 && data != nullptr)
  {
    out.insert(out.end(), data, data + len);
  }

  // Calculate CRC over [LEN_L][LEN_H][CMD][DATA...]
  // CRC input starts at index 1 (after STX) and excludes CRC itself
  const uint8_t crc = calc_crc8(&out[1], out.size() - 1);
  out.push_back(crc);

  return true;
}

void encode_ack(ErrorCode err_code, std::vector<uint8_t>& out)
{
  out.clear();
  out.reserve(5);

  // Response frame: [STX][0x00][0x01][ERR_CODE][CRC8]
  out.push_back(STX);
  out.push_back(0x00);  // LEN_L = 0
  out.push_back(0x01);  // LEN_H = 0 (total length = 1)
  out.push_back(static_cast<uint8_t>(err_code));

  // Calculate CRC over [0x00][0x01][ERR_CODE]
  const uint8_t crc = calc_crc8(&out[1], 3);
  out.push_back(crc);
}

bool verify_frame_crc(const uint8_t* frame, size_t len)
{
  // Minimum frame: STX + LEN_L + LEN_H + CMD + CRC = 5 bytes
  if (len < 5)
  {
    return false;
  }

  // Extract expected CRC (last byte)
  const uint8_t expected_crc = frame[len - 1];

  // Calculate CRC over [LEN_L][LEN_H][CMD][DATA...]
  // This is everything except STX (first byte) and CRC (last byte)
  const uint8_t calculated_crc = calc_crc8(&frame[1], len - 2);

  return expected_crc == calculated_crc;
}

}  // namespace internal
}  // namespace link
}  // namespace v4
