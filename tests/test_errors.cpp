#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <vector>

#include "frame.hpp"
#include "v4/errors.hpp"
#include "v4/panic.h"
#include "v4link/link.hpp"

using namespace v4::link;

namespace
{
struct ErrorFixture
{
  uint8_t memory[1024] = {};
  VmConfig config = {memory, sizeof(memory), nullptr, 0, nullptr};
  Vm* vm = vm_create(&config);
  std::vector<uint8_t> output;
  Link link{vm, write, &output};

  static void write(void* user, const uint8_t* data, size_t size)
  {
    auto* bytes = static_cast<std::vector<uint8_t>*>(user);
    bytes->insert(bytes->end(), data, data + size);
  }

  ~ErrorFixture()
  {
    vm_destroy(vm);
  }

  void send(Command command, const std::vector<uint8_t>& payload)
  {
    REQUIRE(vm != nullptr);
    output.clear();
    std::vector<uint8_t> frame;
    REQUIRE(internal::encode_frame(command, payload.data(), payload.size(), frame));
    for (auto byte : frame)
      link.feed_byte(byte);
  }

  void expect_error(v4_err error)
  {
    REQUIRE(output.size() == 9);
    CHECK(output[0] == STX);
    CHECK(output[1] == 5);
    CHECK(output[2] == 0);
    CHECK(output[3] == static_cast<uint8_t>(ErrorCode::VM_ERROR));
    const uint32_t code = static_cast<uint32_t>(error);
    for (int i = 0; i < 4; ++i)
      CHECK(output[4 + i] == static_cast<uint8_t>(code >> (8 * i)));
    CHECK(internal::verify_frame_crc(output.data(), output.size()));
  }
};
}  // namespace

TEST_CASE_FIXTURE(ErrorFixture, "Raw and v4b EXEC preserve engine errors")
{
  struct Example
  {
    std::vector<uint8_t> code;
    v4_err error;
  };
  const Example examples[] = {{{0x74, 0x73, 0x13, 0x51}, V4_ERR(DivByZero)},
                              {{0x02, 0x51}, V4_ERR(StackUnderflow)},
                              {{0x50, 0xff, 0xff, 0x51}, V4_ERR(InvalidWordIdx)},
                              {{0xff}, V4_ERR(UnknownOp)}};
  for (bool container : {false, true})
  {
    for (const auto& example : examples)
    {
      send(Command::RESET, {});
      std::vector<uint8_t> payload;
      if (container)
      {
        payload = {
            'V', '4', 'B', 'C', 0, 2, 0, 0, static_cast<uint8_t>(example.code.size()),
            0,   0,   0,   0,   0, 0, 0};
      }
      payload.insert(payload.end(), example.code.begin(), example.code.end());
      send(Command::EXEC, payload);
      expect_error(example.error);
      send(Command::PING, {});
      REQUIRE(output.size() == 5);
      CHECK(output[3] == static_cast<uint8_t>(ErrorCode::OK));
    }
  }
}

TEST_CASE_FIXTURE(ErrorFixture, "Registration failure retains the engine code")
{
  static const uint8_t ret[] = {0x51};
  for (int i = 0; i < 256; ++i)
    REQUIRE(vm_register_word(vm, nullptr, ret, sizeof(ret)) == i);
  send(Command::EXEC, {0x51});
  expect_error(V4_ERR(DictionaryFull));
}

TEST_CASE_FIXTURE(ErrorFixture, "Queries report engine failures instead of invented data")
{
  send(Command::QUERY_MEMORY, {0, 4, 0, 0, 4, 0});
  expect_error(V4_ERR(OobMemory));
  send(Command::QUERY_MEMORY, {1, 0, 0, 0, 4, 0});
  expect_error(V4_ERR(Unaligned));
  send(Command::QUERY_WORD, {0xff, 0xff});
  expect_error(V4_ERR(InvalidWordIdx));
  send(Command::QUERY_MEMORY, {});
  REQUIRE(output.size() == 5);
  CHECK(output[3] == static_cast<uint8_t>(ErrorCode::INVALID_FRAME));
}

TEST_CASE_FIXTURE(ErrorFixture, "Returning panic callback permits the error response")
{
  v4_err observed = 0;
  vm_set_panic_handler(
      vm,
      [](void* user, const V4PanicInfo* info)
      { *static_cast<v4_err*>(user) = info->error_code; },
      &observed);
  send(Command::EXEC, {0x74, 0x73, 0x13, 0x51});
  CHECK(observed == V4_ERR(DivByZero));
  expect_error(observed);
}

TEST_CASE_FIXTURE(ErrorFixture, "Negative SYS results remain successful execution")
{
  v4_register_sys_handler(nullptr);
  send(Command::EXEC, {0x73, 0x73, 0x73, 0x73, 0x60, 0x51});
  REQUIRE(output.size() == 8);
  CHECK(output[3] == static_cast<uint8_t>(ErrorCode::OK));
  CHECK(vm_ds_peek_public(vm, 0) == -1);
}
