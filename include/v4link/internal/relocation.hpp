/**
 * @file relocation.hpp
 * @brief Internal bytecode relocation utilities for V4-link
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace v4::link::internal
{

/**
 * @brief Relocate CALL instructions in bytecode by adding offset to word indices
 *
 * Scans through bytecode and adjusts all CALL (0x50) instruction operands by
 * adding the specified offset. This is used during runtime linking to convert
 * file-relative word indices to VM-absolute indices.
 *
 * @param code Pointer to bytecode buffer (will be modified in-place)
 * @param len Length of bytecode in bytes
 * @param offset Value to add to each CALL instruction's word index
 *
 * @note This function modifies the bytecode in-place
 * @note If offset is 0, no changes are made
 * @note Handles all V4 opcode types to correctly skip operands
 */
void relocate_calls(uint8_t* code, size_t len, int offset);

}  // namespace v4::link::internal
