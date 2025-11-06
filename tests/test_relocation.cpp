/**
 * @file test_relocation.cpp
 * @brief Unit tests for bytecode relocation functionality
 *
 * @copyright Copyright 2025 Akihito Kirisaki
 * @license Dual-licensed under MIT or Apache-2.0
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <cstring>
#include <vector>

#include "v4link/internal/relocation.hpp"

using namespace v4::link::internal;

/* ========================================================================= */
/* Basic Relocation Tests                                                   */
/* ========================================================================= */

TEST_CASE("Relocation: Single CALL instruction")
{
  SUBCASE("CALL 0 with offset 5")
  {
    uint8_t code[] = {0x50, 0x00, 0x00};  // CALL 0
    relocate_calls(code, 3, 5);
    CHECK(code[0] == 0x50);  // Opcode unchanged
    CHECK(code[1] == 5);     // Index adjusted to 5
    CHECK(code[2] == 0);
  }

  SUBCASE("CALL 2 with offset 10")
  {
    uint8_t code[] = {0x50, 0x02, 0x00};  // CALL 2
    relocate_calls(code, 3, 10);
    CHECK(code[0] == 0x50);
    CHECK(code[1] == 12);  // 2 + 10
    CHECK(code[2] == 0);
  }

  SUBCASE("CALL 255 with offset 1 (high byte test)")
  {
    uint8_t code[] = {0x50, 0xFF, 0x00};  // CALL 255
    relocate_calls(code, 3, 1);
    CHECK(code[0] == 0x50);
    CHECK(code[1] == 0x00);  // Overflow to high byte
    CHECK(code[2] == 0x01);  // High byte is now 1 (256 = 0x0100)
  }
}

TEST_CASE("Relocation: No relocation when offset is zero")
{
  uint8_t code[] = {0x50, 0x05, 0x00, 0x50, 0x0A, 0x00};
  uint8_t original[6];
  memcpy(original, code, 6);

  relocate_calls(code, 6, 0);

  CHECK(memcmp(code, original, 6) == 0);
}

/* ========================================================================= */
/* Multiple CALL Instructions                                                */
/* ========================================================================= */

TEST_CASE("Relocation: Multiple CALL instructions")
{
  uint8_t code[] = {
      0x50, 0x00, 0x00,  // CALL 0
      0x50, 0x01, 0x00,  // CALL 1
      0x50, 0x02, 0x00   // CALL 2
  };

  relocate_calls(code, 9, 10);

  // CALL 0 → CALL 10
  CHECK(code[0] == 0x50);
  CHECK(code[1] == 10);
  CHECK(code[2] == 0);

  // CALL 1 → CALL 11
  CHECK(code[3] == 0x50);
  CHECK(code[4] == 11);
  CHECK(code[5] == 0);

  // CALL 2 → CALL 12
  CHECK(code[6] == 0x50);
  CHECK(code[7] == 12);
  CHECK(code[8] == 0);
}

/* ========================================================================= */
/* Mixed Opcodes                                                             */
/* ========================================================================= */

TEST_CASE("Relocation: CALL mixed with other opcodes")
{
  SUBCASE("LIT + CALL + RET")
  {
    uint8_t code[] = {
        0x00, 0x64, 0x00, 0x00, 0x00,  // LIT 100
        0x50, 0x00, 0x00,              // CALL 0
        0x51                           // RET
    };

    relocate_calls(code, 9, 5);

    // LIT unchanged
    CHECK(code[0] == 0x00);
    CHECK(code[1] == 0x64);

    // CALL 0 → CALL 5
    CHECK(code[5] == 0x50);
    CHECK(code[6] == 5);
    CHECK(code[7] == 0);

    // RET unchanged
    CHECK(code[8] == 0x51);
  }

  SUBCASE("Multiple opcodes with multiple CALLs")
  {
    uint8_t code[] = {
        0x01,              // DUP
        0x50, 0x01, 0x00,  // CALL 1
        0x10,              // ADD
        0x50, 0x03, 0x00,  // CALL 3
        0x51               // RET
    };

    relocate_calls(code, 9, 10);

    // DUP unchanged
    CHECK(code[0] == 0x01);

    // CALL 1 → CALL 11
    CHECK(code[1] == 0x50);
    CHECK(code[2] == 11);

    // ADD unchanged
    CHECK(code[4] == 0x10);

    // CALL 3 → CALL 13
    CHECK(code[5] == 0x50);
    CHECK(code[6] == 13);

    // RET unchanged
    CHECK(code[8] == 0x51);
  }
}

/* ========================================================================= */
/* Complex Bytecode Patterns                                                 */
/* ========================================================================= */

TEST_CASE("Relocation: Complex bytecode with jumps and literals")
{
  uint8_t code[] = {
      0x00, 0x0A, 0x00, 0x00, 0x00,  // LIT 10
      0x40, 0x05, 0x00,              // JMP +5
      0x50, 0x00, 0x00,              // CALL 0
      0x76, 0x42,                    // LIT_U8 66
      0x50, 0x01, 0x00,              // CALL 1
      0x51                           // RET
  };

  relocate_calls(code, 17, 20);

  // LIT unchanged
  CHECK(code[0] == 0x00);
  CHECK(code[1] == 0x0A);

  // JMP unchanged (relative offset, not word index)
  CHECK(code[5] == 0x40);
  CHECK(code[6] == 0x05);

  // CALL 0 → CALL 20
  CHECK(code[8] == 0x50);
  CHECK(code[9] == 20);

  // LIT_U8 unchanged
  CHECK(code[11] == 0x76);
  CHECK(code[12] == 0x42);

  // CALL 1 → CALL 21
  CHECK(code[13] == 0x50);
  CHECK(code[14] == 21);

  // RET unchanged
  CHECK(code[16] == 0x51);
}

/* ========================================================================= */
/* SYS Instruction Handling                                                  */
/* ========================================================================= */

TEST_CASE("Relocation: SYS instruction (16-byte operand)")
{
  uint8_t code[] = {
      0x60,  // SYS
      // 16-byte SYS operand
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      0x50, 0x00, 0x00,  // CALL 0
      0x51               // RET
  };

  relocate_calls(code, 21, 5);

  // SYS unchanged
  CHECK(code[0] == 0x60);

  // CALL 0 → CALL 5 (at offset 17)
  CHECK(code[17] == 0x50);
  CHECK(code[18] == 5);

  // RET unchanged
  CHECK(code[20] == 0x51);
}

/* ========================================================================= */
/* Edge Cases                                                                */
/* ========================================================================= */

TEST_CASE("Relocation: Edge cases")
{
  SUBCASE("Empty bytecode")
  {
    uint8_t code[1] = {0};
    relocate_calls(code, 0, 10);
    // Should not crash
    CHECK(true);
  }

  SUBCASE("Single byte (incomplete CALL)")
  {
    uint8_t code[] = {0x50};
    relocate_calls(code, 1, 10);
    // Should not crash, opcode is read but no operands available
    CHECK(code[0] == 0x50);
  }

  SUBCASE("Large offset")
  {
    uint8_t code[] = {0x50, 0x00, 0x00};  // CALL 0
    relocate_calls(code, 3, 1000);
    CHECK(code[0] == 0x50);
    CHECK((code[1] | (code[2] << 8)) == 1000);
  }

  SUBCASE("Negative effect (offset larger than initial index - not realistic but safe)")
  {
    uint8_t code[] = {0x50, 0x00, 0x00};  // CALL 0
    relocate_calls(code, 3, -5);
    // Underflow: 0 + (-5) = 0xFFFB (uint16_t)
    uint16_t result = code[1] | (code[2] << 8);
    CHECK(result == 0xFFFB);
  }
}

/* ========================================================================= */
/* Real-world Scenario: LED Shadow Test                                      */
/* ========================================================================= */

TEST_CASE("Relocation: Real-world LED shadowing scenario")
{
  // Simulates led_shadow_test.v4b main code:
  // LED_ON (word 0), LED_ON (word 2 - redefined)
  uint8_t code[] = {
      0x50, 0x00, 0x00,  // CALL 0 (LED_ON first definition)
      0x50, 0x02, 0x00,  // CALL 2 (LED_ON redefined)
      0x51               // RET
  };

  SUBCASE("First load (no offset)")
  {
    uint8_t test_code[7];
    memcpy(test_code, code, 7);
    relocate_calls(test_code, 7, 0);

    // No changes
    CHECK(test_code[1] == 0);
    CHECK(test_code[4] == 2);
  }

  SUBCASE("Second load (offset 4)")
  {
    uint8_t test_code[7];
    memcpy(test_code, code, 7);
    relocate_calls(test_code, 7, 4);

    // CALL 0 → CALL 4
    CHECK(test_code[1] == 4);
    CHECK(test_code[2] == 0);

    // CALL 2 → CALL 6
    CHECK(test_code[4] == 6);
    CHECK(test_code[5] == 0);
  }
}
