# V4-link

Ultra-lightweight bytecode transfer layer for [V4 VM](https://github.com/kirisaki/V4).

## Overview

V4-link provides a minimal communication protocol for transferring compiled bytecode from a host PC to embedded devices over UART. Designed for resource-constrained systems where running a full REPL is impractical.

### Key Features

- **Minimal footprint**: ~1.5KB Flash, ~0.5KB RAM (typical)
- **Platform-agnostic**: No UART dependencies in core library
- **Simple protocol**: Frame-based with CRC-8 error detection
- **Zero runtime allocation**: Memory allocated only during initialization
- **No exceptions**: Safe for embedded environments
- **C++17**: Modern C++ with strict size optimization

### Architecture

```
Host PC (V4-cli):
  Forth source → V4-front (compile) → Bytecode
                                      ↓ UART
Target Device:
  V4-link (receive) → V4 VM (execute) → Result
```

### V4-link vs V4-repl

| Feature          | V4-repl          | V4-link         |
|------------------|------------------|-----------------|
| Purpose          | Interactive REPL | Bytecode loader |
| Forth parser     | ✅ Yes           | ❌ No           |
| Compiler         | ✅ V4-front      | ❌ No           |
| Footprint        | ~15-20KB         | ~1.5KB          |
| Target           | Rich devices     | Minimal devices |

## Protocol Specification

### Frame Format

```
[STX][LEN_L][LEN_H][CMD][DATA...][CRC8]

STX    = 0xA5 (Start of Frame)
LEN    = Payload length (little-endian, 16-bit)
CMD    = Command code
DATA   = Payload (0-512 bytes)
CRC8   = Checksum (polynomial 0x07)
```

### Commands

- **0x10 EXEC**: Execute bytecode
- **0x20 PING**: Connection check
- **0xFF RESET**: Full VM reset

### Response Codes

- **0x00 OK**: Success
- **0x01 ERROR**: General error
- **0x02 INVALID_FRAME**: CRC mismatch
- **0x03 BUFFER_FULL**: Payload too large
- **0x04 VM_ERROR**: VM execution error

## Building

### Prerequisites

- CMake 3.16 or later
- C++17 compiler
- V4 VM library (adjacent directory)

### Build Steps

```bash
# Clone V4 VM if not already present
cd ..
git clone https://github.com/kirisaki/V4.git

# Build V4-link
cd V4-link
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Build Options

- `V4LINK_BUILD_TESTS`: Build unit tests (default: ON)
- `V4LINK_OPTIMIZE_SIZE`: Use `-Os` optimization (default: ON)
- `V4LINK_ENABLE_LTO`: Enable Link Time Optimization (default: OFF)

### Running Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DV4LINK_BUILD_TESTS=ON
cmake --build build -j
cd build && ctest --output-on-failure
```

## Usage

### C++ API Example

```cpp
#include "v4/vm_api.h"
#include "v4link/link.hpp"

// Create VM
uint8_t vm_memory[4096];
VmConfig cfg = {vm_memory, sizeof(vm_memory), nullptr, 0, nullptr};
Vm* vm = vm_create(&cfg);

// Create Link with UART callback
using namespace v4::link;
Link link(vm, [](const uint8_t* data, size_t len) {
  // Send data over UART (platform-specific)
  uart_send(data, len);
});

// Main loop: feed incoming UART bytes
while (true) {
  if (uart_has_data()) {
    uint8_t byte = uart_read_byte();
    link.feed_byte(byte);
  }
}

vm_destroy(vm);
```

### C API Example

```c
#include "v4/vm_api.h"
#include "v4link/link.h"

// UART write callback
void uart_write_callback(void* user, const uint8_t* data, size_t len) {
  // Send data over UART (platform-specific)
  for (size_t i = 0; i < len; ++i) {
    uart_putc(data[i]);
  }
}

int main(void) {
  // Create VM
  uint8_t vm_memory[4096];
  VmConfig cfg = {vm_memory, sizeof(vm_memory), NULL, 0, NULL};
  Vm* vm = vm_create(&cfg);

  // Create Link
  V4Link* link = v4link_create(vm, uart_write_callback, NULL,
                                V4LINK_MAX_PAYLOAD_SIZE);

  // Main loop: feed incoming UART bytes
  while (1) {
    if (uart_has_data()) {
      uint8_t byte = uart_getc();
      v4link_feed_byte(link, byte);
    }
  }

  v4link_destroy(link);
  vm_destroy(vm);
  return 0;
}
```

### Platform Integration Example (CH32V203)

```cpp
#include "v4link/link.hpp"
#include "ch32v20x.h"

class Ch32v203Link {
public:
  Ch32v203Link(Vm* vm)
      : link_(vm, [this](const uint8_t* data, size_t len) {
          uart_send(data, len);
        })
  {
    // Initialize UART hardware
    init_uart();
  }

  void poll() {
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == SET) {
      uint8_t byte = USART_ReceiveData(USART1);
      link_.feed_byte(byte);
    }
  }

private:
  v4::link::Link link_;

  void uart_send(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
      USART_SendData(USART1, data[i]);
    }
  }

  void init_uart() {
    // CH32V203 UART initialization code
    // ...
  }
};
```

### V4-hal Integration Example

```cpp
#include "v4/hal.hpp"
#include "v4link/link.hpp"

// Using V4-hal RAII UART wrapper
v4::hal::HalSystem hal;
hal_uart_config_t cfg = {115200, 8, 1, 0};
v4::hal::Uart uart(0, cfg);

// Create Link
v4::link::Link link(vm, [&uart](const uint8_t* data, size_t len) {
  uart.write(data, len);
});

// Main loop
while (true) {
  if (uart.available() > 0) {
    uint8_t buf[64];
    int n = uart.read(buf, sizeof(buf));
    for (int i = 0; i < n; ++i) {
      link.feed_byte(buf[i]);
    }
  }
}
```

## Memory Requirements

### Typical Flash Usage (ARM Cortex-M, -Os -flto)

- V4 VM core: ~8KB
- V4-link: ~1.5KB
- Platform wrapper: ~0.5KB
- **Total**: ~10KB

### RAM Usage

- V4 VM stacks: ~1KB (256×4B DS + 64×4B RS)
- V4-link buffer: 512B (configurable)
- VM memory: User-defined (typically 2-4KB)
- **Total**: ~4KB typical

## Target Platforms

### Primary: CH32V203

```
CPU:   RISC-V 32-bit @ 144MHz
Flash: 64KB
RAM:   20KB
```

### Secondary: ESP32-C6

```
CPU:   RISC-V 32-bit @ 160MHz
Flash: 4MB
RAM:   512KB
```

## Directory Structure

```
V4-link/
├── CMakeLists.txt          # Main build configuration
├── README.md               # This file
├── LICENSE-MIT             # MIT License
├── LICENSE-APACHE          # Apache 2.0 License
├── include/v4link/
│   ├── link.hpp            # Public API
│   └── protocol.hpp        # Protocol definitions
├── src/
│   ├── link.cpp            # Link implementation
│   ├── frame.cpp           # Frame encoding/decoding
│   ├── frame.hpp           # Frame internal API
│   ├── crc8.cpp            # CRC-8 calculation
│   └── crc8.hpp            # CRC-8 internal API
└── tests/
    ├── CMakeLists.txt      # Test configuration
    └── test_link.cpp       # Unit tests
```

## API Reference

### C++ API

#### `v4::link::Link`

Main class for bytecode reception and execution.

#### Constructor

```cpp
Link(Vm* vm, UartWriteFn uart_write, size_t buffer_size = 512);
```

- `vm`: Pointer to initialized V4 VM instance
- `uart_write`: Callback for UART transmission
- `buffer_size`: Maximum bytecode buffer size

#### Methods

```cpp
void feed_byte(uint8_t byte);
```
Process one received byte from UART.

```cpp
void reset();
```
Reset VM to initial state (clear stacks and dictionary).

```cpp
size_t buffer_capacity() const;
```
Get maximum buffer size in bytes.

### C API

#### `v4link_create()`

```c
V4Link* v4link_create(Vm* vm, v4link_uart_write_fn uart_write,
                      void* user, size_t buffer_size);
```
Create a new Link instance.

#### `v4link_destroy()`

```c
void v4link_destroy(V4Link* link);
```
Destroy Link instance and free resources.

#### `v4link_feed_byte()`

```c
void v4link_feed_byte(V4Link* link, uint8_t byte);
```
Process one received byte from UART.

#### `v4link_reset()`

```c
void v4link_reset(V4Link* link);
```
Reset VM to initial state.

#### `v4link_strerror()`

```c
const char* v4link_strerror(v4link_error_t err);
```
Get error message string for an error code.

## Related Projects

- [V4](https://github.com/kirisaki/V4) - Core VM
- [V4-front](https://github.com/kirisaki/V4-front) - Forth compiler
- [V4-repl](https://github.com/kirisaki/V4-repl) - Interactive REPL
- [V4-hal](https://github.com/kirisaki/V4-hal) - Hardware abstraction layer
- [V4-ports](https://github.com/kirisaki/V4-ports) - Platform implementations
- V4-cli - Host CLI tool (planned)

## License

Licensed under either of:

- MIT License ([LICENSE-MIT](LICENSE-MIT))
- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))

at your option.

## Contributing

Contributions are welcome! Please ensure:

1. Code follows existing style (`.clang-format`)
2. All tests pass (`ctest`)
3. No exceptions or RTTI
4. Minimal footprint maintained

Copyright 2025 Akihito Kirisaki
