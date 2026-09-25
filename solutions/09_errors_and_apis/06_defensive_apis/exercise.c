// Solution -- 09.06 Validate at the boundary, and validate ALL of it

#include <mect/mect.h>

#include <stdint.h>

enum cmd_status {
  CMD_OK = 0,
  CMD_ERR_UNKNOWN,
  CMD_ERR_RANGE,
};

// The wire protocol under test:
//   0x10..0x13  set brightness of channel 0..3; value must be 50..200
//   0x20        ping (value ignored)
//   everything else is unknown and must change NOTHING.
#define CMD_SET_BASE 0x10
#define CMD_SET_LAST 0x13
#define CMD_PING 0x20
#define VALUE_MIN 50
#define VALUE_MAX 200

static struct {
  uint8_t channels[4];
  uint8_t canary[32]; // the memory "after the channels" (00.03's trick)
} state;

static void state_reset(void) {
  for (unsigned i = 0; i < sizeof state.channels; ++i) {
    state.channels[i] = 0;
  }
  for (unsigned i = 0; i < sizeof state.canary; ++i) {
    state.canary[i] = 0xA5;
  }
}

enum cmd_status command_apply(uint8_t cmd, uint8_t value) {
  // This function IS the trust boundary: `cmd` and `value` came off a wire
  // and can be any of the 65,536 combinations. Everything behind this
  // function trusts what it lets through -- so the checks here are total,
  // and the checks BEHIND it can be asserts (09.05), not repeats.
  if (cmd == CMD_PING) {
    return CMD_OK;
  }
  if (cmd < CMD_SET_BASE || cmd > CMD_SET_LAST) {
    return CMD_ERR_UNKNOWN;
  }
  if (value < VALUE_MIN || value > VALUE_MAX) { // inclusive bounds: 50 and
    return CMD_ERR_RANGE;                       // 200 are both legal
  }
  state.channels[cmd - CMD_SET_BASE] = value;
  return CMD_OK;
}

TEST("valid commands land on the right channel") {
  state_reset();
  CHECK_EQ(command_apply(0x10, 100), CMD_OK);
  CHECK_EQ(command_apply(0x13, 51), CMD_OK);
  CHECK_EQ(state.channels[0], 100u);
  CHECK_EQ(state.channels[3], 51u);
  CHECK_EQ(command_apply(0x20, 0), CMD_OK); // ping ignores its value
}

TEST("the range bounds are inclusive") {
  state_reset();
  CHECK_EQ(command_apply(0x11, 49), CMD_ERR_RANGE);
  CHECK_EQ(command_apply(0x11, 50), CMD_OK); // the minimum is legal
  CHECK_EQ(state.channels[1], 50u);
  CHECK_EQ(command_apply(0x11, 200), CMD_OK); // so is the maximum
  CHECK_EQ(state.channels[1], 200u);
  CHECK_EQ(command_apply(0x11, 201), CMD_ERR_RANGE);
  CHECK_EQ(state.channels[1], 200u); // rejected values change nothing
}

TEST("every possible command byte, because the wire will send them all") {
  state_reset();
  for (unsigned c = 0; c <= 0xFF; ++c) {
    const uint8_t cmd = (uint8_t)c;
    const enum cmd_status st = command_apply(cmd, 100);
    if ((cmd >= CMD_SET_BASE && cmd <= CMD_SET_LAST) || cmd == CMD_PING) {
      CHECK_EQ(st, CMD_OK);
    } else {
      CHECK_EQ(st, CMD_ERR_UNKNOWN);
    }
  }
  // Nothing above may have written past the four real channels.
  for (unsigned i = 0; i < sizeof state.canary; ++i) {
    CHECK_EQ(state.canary[i], 0xA5u);
  }
}
