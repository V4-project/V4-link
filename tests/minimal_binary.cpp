/**
 * @file minimal_binary.cpp
 * @brief Minimal executable to measure actual binary size
 *
 * This program contains the bare minimum code to use V4-link,
 * allowing us to measure the actual minimum binary size without
 * test frameworks or other overhead.
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#include "v4/vm_api.h"
#include "v4link/link.hpp"

// Minimal UART write function
static void uart_write(const uint8_t* data, size_t len)
{
  // In a real embedded system, this would write to UART hardware
  // For this minimal binary, we just ignore the output
  (void)data;
  (void)len;
}

int main()
{
  // Create VM with minimal memory
  uint8_t vm_memory[512] = {0};
  VmConfig cfg = {vm_memory, sizeof(vm_memory), nullptr, 0, nullptr};
  Vm* vm = vm_create(&cfg);

  if (vm == nullptr)
  {
    return 1;
  }

  // Create link instance
  v4::link::Link link(vm, uart_write);

  // Process a single PING command frame
  // Frame format: [STX][LEN_L][LEN_H][CMD][CRC]
  // PING with empty payload: 0x02 0x00 0x00 0x01 (CRC calculated)
  const uint8_t ping_frame[] = {0x02, 0x00, 0x00, 0x01, 0x03};

  for (size_t i = 0; i < sizeof(ping_frame); i++)
  {
    link.feed_byte(ping_frame[i]);
  }

  // Clean up
  vm_destroy(vm);

  return 0;
}
