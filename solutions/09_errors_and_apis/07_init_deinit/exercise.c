// Solution -- 09.07 init/deinit: a lifecycle the module itself enforces

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

enum bus_status {
  BUS_OK = 0,
  BUS_ERR_STATE, // called in the wrong lifecycle state
  BUS_ERR_BAD_ARG,
};

// A pretend device on the bus: eight registers with known contents.
static const uint8_t fake_device_regs[8] = {0x1D, 0x00, 0x42, 0x07,
                                            0xA0, 0x55, 0x00, 0xFF};

// All module state in ONE struct (06.05): the lifecycle flag is part of the
// state it guards.
static struct {
  bool initialised;
  uint16_t addr;
  uint32_t reads; // statistics; also proof deinit really cleaned up
} bus;

enum bus_status bus_init(uint16_t addr) {
  if (addr == 0) {
    return BUS_ERR_BAD_ARG; // address 0 is the general-call address
  }
  if (bus.initialised) {
    return BUS_ERR_STATE; // init is once; re-init requires deinit first
  }
  bus.addr = addr;
  bus.reads = 0;
  bus.initialised = true; // LAST: nothing observes a half-built module
  return BUS_OK;
}

enum bus_status bus_read(uint8_t reg, uint8_t *out) {
  if (!bus.initialised) {
    return BUS_ERR_STATE; // not "return 0": silence here becomes a wrong
  } // sensor value forty modules downstream
  if (reg >= sizeof fake_device_regs) {
    return BUS_ERR_BAD_ARG;
  }
  ++bus.reads;
  *out = fake_device_regs[reg];
  return BUS_OK;
}

enum bus_status bus_deinit(void) {
  // Idempotent by design: deinit on a deinitialised module is a no-op
  // success. Shutdown paths call deinit from error handling (09.03), and
  // error handling must not need to know how far init got.
  bus.initialised = false;
  return BUS_OK;
}

TEST("operations before init are refused, not improvised") {
  (void)bus_deinit(); // known state, whatever ran before us
  uint8_t v = 0xEE;
  CHECK_EQ(bus_read(2, &v), BUS_ERR_STATE);
  CHECK_EQ(v, 0xEEu); // and nothing was written (09.02)
}

TEST("init exactly once; a second init is a state error") {
  (void)bus_deinit();
  CHECK_EQ(bus_init(0x76), BUS_OK);
  CHECK_EQ(bus_init(0x76), BUS_ERR_STATE);
  CHECK_EQ(bus_init(0), BUS_ERR_BAD_ARG);
}

TEST("between init and deinit, the module works") {
  (void)bus_deinit();
  CHECK_EQ(bus_init(0x76), BUS_OK);
  uint8_t v = 0;
  CHECK_EQ(bus_read(2, &v), BUS_OK);
  CHECK_EQ(v, 0x42u);
  CHECK_EQ(bus_read(9, &v), BUS_ERR_BAD_ARG); // bad register, good state
}

TEST("deinit is idempotent, and the lifecycle restarts cleanly") {
  (void)bus_deinit();
  CHECK_EQ(bus_init(0x76), BUS_OK);
  CHECK_EQ(bus_deinit(), BUS_OK);
  CHECK_EQ(bus_deinit(), BUS_OK); // twice is fine -- shutdown paths overlap

  uint8_t v = 0;
  CHECK_EQ(bus_read(2, &v), BUS_ERR_STATE); // dead again

  CHECK_EQ(bus_init(0x3C), BUS_OK); // and the module is reusable
  CHECK_EQ(bus_read(0, &v), BUS_OK);
  CHECK_EQ(v, 0x1Du);
  CHECK_EQ(bus_deinit(), BUS_OK);
}
