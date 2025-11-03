# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Added minimal binary size test program (`tests/minimal_binary.cpp`)
  - Measures actual embedded binary size without test framework overhead
  - Integrated into `make size` target with detailed size breakdown
  - Shows binary composition (text, data, bss sections)

### Changed

- **BREAKING**: Changed `UartWriteFn` callback signature to use C-style function pointer
  - Old: `std::function<void(const uint8_t*, size_t)>`
  - New: `void (*)(void* user, const uint8_t*, size_t)`
  - Added user context pointer parameter for better C interoperability
  - Removed `<functional>` header dependency
  - Reduces binary size by ~900 bytes
- Updated `Link` constructor to accept user context parameter
  - Signature: `Link(Vm* vm, UartWriteFn uart_write, void* user = nullptr, size_t buffer_size = MAX_PAYLOAD_SIZE)`
  - User context is passed to callback on each invocation

### Improved

- Enabled Link Time Optimization (LTO) by default in `make size` target
  - Fixed LTO detection to properly specify C/CXX languages in CMake
  - Enabled LTO for V4 VM, mock_hal, and V4-link libraries
  - Reduces minimal binary from 23.6KB to 17.3KB (26.7% reduction)
  - Binary size breakdown with LTO enabled:
    - text: 13,887 bytes (code)
    - data: 680 bytes (initialized data)
    - bss: 3,136 bytes (uninitialized data)

## [0.2.0] - 2025-11-02

### Changed

- **BREAKING**: Extended EXEC response to include registered word index for REPL support
  - Old format: `[STX][0x01][0x00][ERR_CODE][CRC8]` (5 bytes)
  - New format: `[STX][LEN_L][LEN_H][ERR_CODE][WORD_COUNT][WORD_IDX...][CRC8]` (variable length)
  - LEN = 2 + WORD_COUNT * 2
  - WORD_COUNT: Number of words registered (always 1 for single bytecode execution)
  - WORD_IDX: 16-bit little-endian word index for each registered word
  - This enables host-side REPL compilers to track word definitions registered on device VM
  - Updated tests to reflect new response format

### Fixed

- Fixed critical bug where bytecode pointers became invalid after frame processing
  - `vm_register_word` stores bytecode pointer without copying, requiring persistent storage
  - Added `bytecode_storage_` vector to maintain bytecode lifetime for registered words
  - Bytecode storage is cleared on VM reset to prevent memory leaks
  - This fix enables REPL word definitions to work correctly
- Fixed EXEC command failing on word definitions
  - Word definition bytecode (e.g., `[DUP, MUL, RET]`) should only be registered, not executed
  - Changed to ignore execution errors and always return word index
  - Execution errors are expected for word definitions which require stack setup
  - This allows REPL to properly register word definitions for later use

## [0.1.1] - 2025-11-01

### Fixed

- Corrected response frame encoding documentation to accurately reflect little-endian byte order

## [0.1.0] - 2025-11-01

### Added

- Initial release of V4-link bytecode transfer layer
- Minimal frame-based protocol for UART communication
- CRC-8 error detection (polynomial 0x07)
- Three core commands: EXEC, PING, RESET
- Platform-agnostic design using callback-based UART abstraction
- C++ API (`v4link::Link` class) with modern C++17 features
- C API (`v4link_*` functions) for C-compatible integration
- State machine for byte-by-byte frame reception
- Automatic bytecode registration and execution in V4 VM
- Comprehensive test suite using doctest
- CMake build system with V4 VM integration
- Error codes defined via `.def` file (V4-style)
- Memory-efficient design (~1.5KB Flash, ~512B RAM typical)
- Zero runtime allocation (memory allocated only during initialization)
- No exceptions, no RTTI (embedded-friendly)
- Documentation and usage examples in README.md
- MIT + Apache-2.0 dual licensing

### Protocol Specification

- Frame format: `[STX][LEN_L][LEN_H][CMD][DATA...][CRC8]`
- Maximum payload size: 512 bytes (configurable)
- Commands:
  - `0x10 EXEC`: Execute bytecode immediately
  - `0x20 PING`: Connection check
  - `0xFF RESET`: Full VM reset
- Response codes:
  - `0x00 OK`: Success
  - `0x01 ERROR`: General error
  - `0x02 INVALID_FRAME`: CRC mismatch
  - `0x03 BUFFER_FULL`: Payload too large
  - `0x04 VM_ERROR`: VM execution error

### Target Platforms

- Primary: CH32V203 (RISC-V, 64KB Flash, 20KB RAM)
- Secondary: ESP32-C6 (RISC-V, 4MB Flash, 512KB RAM)
- Host: POSIX systems (Linux, macOS) for testing

[Unreleased]: https://github.com/V4-project/V4-link/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/V4-project/V4-link/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/V4-project/V4-link/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/V4-project/V4-link/releases/tag/v0.1.0
