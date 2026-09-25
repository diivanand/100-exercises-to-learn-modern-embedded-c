// =============================================================================
//  09.07 -- init/deinit: a lifecycle the module itself enforces
// =============================================================================
//
//  Every driver in this course's later chapters has the same skeleton:
//
//      bus_init(...)      once, before anything else
//      bus_read(...)      any number of times, only while initialised
//      bus_deinit()       reverses init; afterwards the module is dead
//
//  The lifecycle is a contract, and the module ENFORCES it -- because the
//  callers will get it wrong. Startup code races, shutdown paths overlap,
//  and error unwinding (09.03) calls deinit without knowing how far init
//  got. The rules that survive contact with real call sites:
//
//   - a `initialised` flag guards every operation: called dead, a function
//     returns BUS_ERR_STATE. Not zero, not a guess -- an error. A module
//     that "sort of works" uninitialised produces plausible garbage, and
//     plausible garbage sails through integration testing;
//   - init on an initialised module is BUS_ERR_STATE, not a silent
//     re-init that yanks the state from under the current user;
//   - deinit is IDEMPOTENT: calling it twice is a no-op success, because
//     two shutdown paths WILL both call it;
//   - after deinit, init works again. The lifecycle is a cycle.
//
//  THE STARTER got the flag "clever": a reference COUNT that deinit
//  decrements blindly. One double-deinit later the count is -1, init sees
//  "someone is still using this" forever, and the module is bricked until
//  reboot. Watch the last test walk into exactly that. The starter's read
//  also skips the guard -- before init it happily returns register
//  contents, which is worse than failing: it is WRONG QUIETLY.
//
//  (Why a file-scope singleton and not a struct the caller owns? For a
//  one-per-chip peripheral, the singleton matches the hardware -- but keep
//  the state in ONE struct anyway, as here, so promoting it to multiple
//  instances later is mechanical. 06.05 made that argument.)
//
//  TASK
//    Replace the refcount with an honest `initialised` flag, guard every
//    operation, and make deinit idempotent. Do not change the tests.
//
//  RUN IT
//    ./mec test 09_07
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

enum bus_status {
  BUS_OK = 0,
  BUS_ERR_STATE,   // called in the wrong lifecycle state
  BUS_ERR_BAD_ARG,
};

// A pretend device on the bus: eight registers with known contents.
static const uint8_t fake_device_regs[8] = {0x1D, 0x00, 0x42, 0x07,
                                            0xA0, 0x55, 0x00, 0xFF};

static struct {
  int users; // TODO: a refcount that only ever counts one user is a flag
             // with extra failure modes
  uint16_t addr;
  uint32_t reads;
} bus;

enum bus_status bus_init(uint16_t addr) {
  if (addr == 0) {
    return BUS_ERR_BAD_ARG; // address 0 is the general-call address
  }
  if (bus.users != 0) { // TODO: after a double deinit this is -1, forever
    return BUS_ERR_STATE;
  }
  ++bus.users;
  bus.addr = addr;
  bus.reads = 0;
  return BUS_OK;
}

enum bus_status bus_read(uint8_t reg, uint8_t *out) {
  // TODO: no guard -- before init this "works", which is the worst thing
  // it could do.
  if (reg >= sizeof fake_device_regs) {
    return BUS_ERR_BAD_ARG;
  }
  ++bus.reads;
  *out = fake_device_regs[reg];
  return BUS_OK;
}

enum bus_status bus_deinit(void) {
  --bus.users; // TODO: deinit on a dead module digs below zero
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
