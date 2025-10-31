/**
 * @file link_c_api.cpp
 * @brief V4-link C API implementation
 *
 * C wrapper for the C++ Link class.
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include <new>

#include "v4link/link.h"
#include "v4link/link.hpp"

using namespace v4::link;

/* ========================================================================= */
/* Internal wrapper structure                                                */
/* ========================================================================= */

struct V4Link
{
  Link* cpp_link;
  void* user;
  v4link_uart_write_fn uart_write;

  V4Link(Vm* vm, v4link_uart_write_fn write_fn, void* user_ctx, size_t buffer_size)
      : cpp_link(nullptr), user(user_ctx), uart_write(write_fn)
  {
    // Create C++ Link with lambda that wraps the C callback
    cpp_link = new (std::nothrow) Link(
        vm,
        [this](const uint8_t* data, size_t len)
        {
          if (uart_write)
          {
            uart_write(user, data, len);
          }
        },
        buffer_size);
  }

  ~V4Link()
  {
    delete cpp_link;
  }
};

/* ========================================================================= */
/* Error message strings                                                     */
/* ========================================================================= */

const char* v4link_strerror(v4link_error_t err)
{
  switch (err)
  {
#define ERR(name, val, msg) \
  case V4LINK_ERR_##name:   \
    return msg;
#include "v4link/errors.def"
#undef ERR
    default:
      return "unknown error";
  }
}

/* ========================================================================= */
/* Lifecycle functions                                                       */
/* ========================================================================= */

V4Link* v4link_create(Vm* vm, v4link_uart_write_fn uart_write, void* user,
                      size_t buffer_size)
{
  if (vm == nullptr || uart_write == nullptr)
  {
    return nullptr;
  }

  if (buffer_size == 0)
  {
    buffer_size = V4LINK_MAX_PAYLOAD_SIZE;
  }

  V4Link* link = new (std::nothrow) V4Link(vm, uart_write, user, buffer_size);
  if (link == nullptr || link->cpp_link == nullptr)
  {
    delete link;
    return nullptr;
  }

  return link;
}

void v4link_destroy(V4Link* link)
{
  delete link;
}

/* ========================================================================= */
/* Operation functions                                                       */
/* ========================================================================= */

void v4link_feed_byte(V4Link* link, uint8_t byte)
{
  if (link && link->cpp_link)
  {
    link->cpp_link->feed_byte(byte);
  }
}

void v4link_reset(V4Link* link)
{
  if (link && link->cpp_link)
  {
    link->cpp_link->reset();
  }
}

size_t v4link_buffer_capacity(const V4Link* link)
{
  if (link && link->cpp_link)
  {
    return link->cpp_link->buffer_capacity();
  }
  return 0;
}
