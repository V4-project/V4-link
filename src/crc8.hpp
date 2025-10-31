/**
 * @file crc8.hpp
 * @brief CRC-8 checksum calculation (internal)
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
namespace internal
{

/**
 * @brief Calculate CRC-8 checksum
 *
 * Uses polynomial 0x07 (x^8 + x^2 + x + 1) with initial value 0x00.
 *
 * @param data Pointer to data buffer
 * @param len  Length of data in bytes
 * @return CRC-8 checksum value
 */
uint8_t calc_crc8(const uint8_t* data, size_t len);

}  // namespace internal
}  // namespace link
}  // namespace v4
