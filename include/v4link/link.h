/**
 * @file link.h
 * @brief V4-link C API
 *
 * C-compatible interface for V4-link bytecode transfer layer.
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "v4/vm_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* ========================================================================= */
  /* Protocol constants                                                        */
  /* ========================================================================= */

  /** @brief Start of frame marker */
#define V4LINK_STX 0xA5

  /** @brief Maximum payload size */
#define V4LINK_MAX_PAYLOAD_SIZE 512

  /* ========================================================================= */
  /* Command codes                                                             */
  /* ========================================================================= */

  typedef enum
  {
    V4LINK_CMD_EXEC = 0x10,  /**< Execute bytecode */
    V4LINK_CMD_PING = 0x20,  /**< Ping command */
    V4LINK_CMD_RESET = 0xFF, /**< Reset VM */
  } v4link_command_t;

  /* ========================================================================= */
  /* Error codes                                                               */
  /* ========================================================================= */

  typedef enum
  {
#define ERR(name, val, msg) V4LINK_ERR_##name = val,
#include "v4link/errors.def"
#undef ERR
  } v4link_error_t;

  /**
   * @brief Get error message string
   * @param err Error code
   * @return Error message (static string)
   */
  const char* v4link_strerror(v4link_error_t err);

  /* ========================================================================= */
  /* Link handle                                                               */
  /* ========================================================================= */

  /** @brief Opaque handle to Link instance */
  typedef struct V4Link V4Link;

  /**
   * @brief UART write callback function type
   *
   * @param user User-defined context pointer
   * @param data Data buffer to write
   * @param len  Number of bytes to write
   */
  typedef void (*v4link_uart_write_fn)(void* user, const uint8_t* data, size_t len);

  /* ========================================================================= */
  /* Lifecycle functions                                                       */
  /* ========================================================================= */

  /**
   * @brief Create a new Link instance
   *
   * @param vm           Pointer to initialized V4 VM instance
   * @param uart_write   UART write callback function
   * @param user         User context pointer (passed to uart_write)
   * @param buffer_size  Maximum bytecode buffer size (default: 512 bytes)
   * @return Pointer to Link instance, or NULL on allocation failure
   */
  V4Link* v4link_create(Vm* vm, v4link_uart_write_fn uart_write, void* user,
                        size_t buffer_size);

  /**
   * @brief Destroy Link instance and free resources
   * @param link Link instance (NULL-safe)
   */
  void v4link_destroy(V4Link* link);

  /* ========================================================================= */
  /* Operation functions                                                       */
  /* ========================================================================= */

  /**
   * @brief Process one received byte
   *
   * Call this function from the main loop whenever a byte is received
   * from UART. The state machine processes the byte and automatically
   * handles complete frames.
   *
   * @param link Link instance
   * @param byte Received byte
   */
  void v4link_feed_byte(V4Link* link, uint8_t byte);

  /**
   * @brief Reset VM to initial state
   *
   * Calls vm_reset() to clear stacks and dictionary.
   * Does not reset the frame reception state machine.
   *
   * @param link Link instance
   */
  void v4link_reset(V4Link* link);

  /**
   * @brief Get current buffer capacity
   *
   * @param link Link instance
   * @return Maximum bytecode buffer size in bytes
   */
  size_t v4link_buffer_capacity(const V4Link* link);

#ifdef __cplusplus
} /* extern "C" */
#endif
