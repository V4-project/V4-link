/**
 * @file link.hpp
 * @brief V4-link main API
 *
 * Lightweight bytecode transfer layer for V4 VM.
 * Provides minimal protocol handling for embedded systems.
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "v4/vm_api.h"
#include "v4link/protocol.hpp"

namespace v4
{
namespace link
{

/**
 * @brief V4-link bytecode receiver and executor
 *
 * Handles frame reception, protocol parsing, and VM bytecode execution.
 * Platform-agnostic design using callback-based UART abstraction.
 *
 * Example usage:
 * @code
 * // Create VM
 * VmConfig cfg = {mem, sizeof(mem), nullptr, 0, nullptr};
 * Vm* vm = vm_create(&cfg);
 *
 * // Define UART write callback
 * void uart_write_fn(void* user, const uint8_t* data, size_t len) {
 *   uart_send(data, len);  // Platform-specific UART send
 * }
 *
 * // Create Link with UART callback
 * Link link(vm, uart_write_fn, nullptr);
 *
 * // Main loop: feed incoming UART bytes
 * while (true) {
 *   if (uart_has_data()) {
 *     uint8_t byte = uart_read_byte();
 *     link.feed_byte(byte);
 *   }
 * }
 * @endcode
 */
class Link
{
 public:
  /**
   * @brief UART write callback function type
   *
   * User-provided function to send data over UART.
   * Must be non-blocking or have minimal blocking time.
   *
   * @param user User context pointer passed during construction
   * @param data Pointer to data buffer
   * @param len  Number of bytes to write
   */
  using UartWriteFn = void (*)(void* user, const uint8_t*, size_t);

  /**
   * @brief Construct Link instance
   *
   * @param vm           Pointer to initialized V4 VM instance
   * @param uart_write   Callback function for UART transmission
   * @param user         User context pointer passed to callback (can be nullptr)
   * @param buffer_size  Maximum bytecode buffer size (default: 512 bytes)
   */
  Link(Vm* vm, UartWriteFn uart_write, void* user = nullptr,
       size_t buffer_size = MAX_PAYLOAD_SIZE);

  /**
   * @brief Process one received byte
   *
   * Call this function from the main loop whenever a byte is received
   * from UART. The state machine processes the byte and automatically
   * handles complete frames.
   *
   * @param byte Received byte
   */
  void feed_byte(uint8_t byte);

  /**
   * @brief Reset VM to initial state
   *
   * Calls vm_reset() to clear stacks and dictionary.
   * Does not reset the frame reception state machine.
   */
  void reset();

  /**
   * @brief Get current buffer capacity
   *
   * @return Maximum bytecode buffer size in bytes
   */
  size_t buffer_capacity() const
  {
    return buffer_.capacity();
  }

 private:
  /**
   * @brief Frame reception state machine
   */
  enum class State
  {
    WAIT_STX,    // Waiting for start of frame (0xA5)
    WAIT_LEN_L,  // Waiting for length low byte
    WAIT_LEN_H,  // Waiting for length high byte
    WAIT_CMD,    // Waiting for command byte
    WAIT_DATA,   // Receiving payload data
    WAIT_CRC,    // Waiting for CRC byte
  };

  /**
   * @brief Handle complete frame
   *
   * Called when a complete frame has been received.
   * Verifies CRC, dispatches command, and sends response.
   */
  void handle_frame();

  /**
   * @brief Send ACK/NAK response
   *
   * @param code     Error code to send
   * @param data     Optional payload data (nullptr for standard response)
   * @param data_len Payload length in bytes (0 for standard response)
   */
  void send_ack(ErrorCode code, const uint8_t* data = nullptr, size_t data_len = 0);

  /**
   * @brief Handle CMD_EXEC command
   */
  void handle_cmd_exec();

  /**
   * @brief Handle CMD_PING command
   */
  void handle_cmd_ping();

  /**
   * @brief Handle CMD_RESET command
   */
  void handle_cmd_reset();

  /**
   * @brief Handle CMD_QUERY_STACK command
   */
  void handle_cmd_query_stack();

  /**
   * @brief Handle CMD_QUERY_MEMORY command
   */
  void handle_cmd_query_memory();

  /**
   * @brief Handle CMD_QUERY_WORD command
   */
  void handle_cmd_query_word();

  Vm* vm_;                       ///< V4 VM instance
  UartWriteFn uart_write_;       ///< UART write callback
  void* user_context_;           ///< User context for callback
  std::vector<uint8_t> buffer_;  ///< Frame reception buffer
  size_t pos_;                   ///< Current position in buffer

  State state_;         ///< State machine state
  uint16_t frame_len_;  ///< Expected payload length
  uint8_t cmd_;         ///< Current command code

  std::vector<std::vector<uint8_t>>
      bytecode_storage_;  ///< Persistent bytecode storage for registered words
};

}  // namespace link
}  // namespace v4
