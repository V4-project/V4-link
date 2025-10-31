/**
 * @file crc8.cpp
 * @brief CRC-8 checksum implementation
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include "crc8.hpp"

#include "v4link/protocol.hpp"

namespace v4
{
namespace link
{
namespace internal
{

uint8_t calc_crc8(const uint8_t* data, size_t len)
{
  uint8_t crc = 0x00;

  for (size_t i = 0; i < len; ++i)
  {
    crc ^= data[i];

    for (int bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x80)
      {
        crc = (crc << 1) ^ CRC8_POLY;
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

}  // namespace internal
}  // namespace link
}  // namespace v4
