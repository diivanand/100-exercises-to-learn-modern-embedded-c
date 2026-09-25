// =============================================================================
//  11.07 -- Volatile orders volatile; everything else needs a fence
// =============================================================================
//
//  11.01 listed what volatile does not give you. Here is the sharpest of
//  those edges. The pattern: fill a buffer, then raise a flag that tells an
//  ISR (or a DMA engine, or the other core) the buffer is ready.
//
//      mb->payload[0] = ...;      // ordinary stores
//      mb->payload[1] = ...;
//      mb->ready = 1;             // volatile store: "go"
//
//  Volatile promises the `ready` store happens, and happens in order
//  RELATIVE TO OTHER VOLATILE ACCESSES. The payload stores are not
//  volatile. The compiler is free to sink them BELOW the flag store -- it
//  can prove no C code observes the difference, because the ISR that will
//  read the payload is not called from here. Interrupt fires between the
//  reordered stores: the ISR reads a "ready" mailbox full of garbage. A
//  bug that appears only at -O2, only sometimes, and never under the
//  debugger. (11.01's hoisted load was the compiler deleting a read you
//  needed; this is it MOVING a write you needed in place.)
//
//  The standard tool is C11's compiler fence:
//
//      atomic_signal_fence(memory_order_release);
//
//  "signal" because ISO C's model for same-thread asynchrony is the signal
//  handler -- an ISR is the same shape. It compiles to ZERO instructions;
//  its entire effect is on the optimiser: ordinary stores above it may not
//  move below it. (The GNU spelling `asm volatile("" ::: "memory")` does
//  the same and predates C11; you will see both in real code.) On a single
//  core that is ENOUGH -- an ISR on the same CPU sees stores in program
//  order once the compiler stops shuffling them. Between two cores or with
//  an unsynchronised DMA master you need real hardware barriers and the
//  <stdatomic.h> machinery -- chapter 12 takes over from there.
//
//  TESTING intent, not luck: forcing clang to exhibit the reorder would tie
//  the test to a compiler version, so the mailbox routes every store
//  through a logging helper -- a poor man's bus analyser -- and the test
//  reads the ORDER off the log (see CONTRIBUTING on deterministic
//  concurrency tests). The starter's order is wrong in the source itself;
//  the prose above is about how the compiler can wrong it FOR you even
//  when the source is right, which is why the fix carries the fence too.
//
//  TASK
//    Fix `mailbox_post`: payload first, then the fence, then the flag. Do
//    not change the bus analyser or the tests.
//
//  RUN IT
//    ./mec test 11_07
//
// =============================================================================

#include <mect/mect.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

// --- the bus analyser (given) --------------------------------------------------
// Every store the mailbox code performs goes through a helper that logs its
// kind, so the tests can read the ORDER of stores off the log -- a poor
// man's logic analyser.

enum store_kind { STORE_PAYLOAD = 1, STORE_FLAG = 2 };

static uint8_t bus_log[8];
static size_t bus_log_count;

static void bus_reset(void) {
  bus_log_count = 0;
}

static void bus_record(enum store_kind kind) {
  if (bus_log_count < sizeof bus_log) {
    bus_log[bus_log_count++] = (uint8_t)kind;
  }
}

// --- the mailbox -----------------------------------------------------------------

struct mailbox {
  uint8_t payload[4];      // ordinary memory
  volatile uint32_t ready; // the "go" flag an ISR polls
};

static void store_payload_byte(struct mailbox *mb, size_t i, uint8_t value) {
  mb->payload[i] = value;
  bus_record(STORE_PAYLOAD);
}

static void store_ready(struct mailbox *mb) {
  mb->ready = 1;
  bus_record(STORE_FLAG);
}

void mailbox_post(struct mailbox *mb, const uint8_t data[4]) {
  // TODO: the flag goes up before the payload exists, and there is no
  // fence to stop the optimiser doing the same thing to correct-looking
  // source. An ISR arriving "now" reads garbage with a clear conscience.
  store_ready(mb);
  for (size_t i = 0; i < 4; ++i) {
    store_payload_byte(mb, i, data[i]);
  }
}

TEST("the flag is the LAST thing stored") {
  struct mailbox mb = {0};
  bus_reset();

  mailbox_post(&mb, (const uint8_t[]){0xDE, 0xAD, 0xBE, 0xEF});

  REQUIRE(bus_log_count == 5);
  CHECK_EQ(bus_log[0], (unsigned)STORE_PAYLOAD);
  CHECK_EQ(bus_log[1], (unsigned)STORE_PAYLOAD);
  CHECK_EQ(bus_log[2], (unsigned)STORE_PAYLOAD);
  CHECK_EQ(bus_log[3], (unsigned)STORE_PAYLOAD);
  CHECK_EQ(bus_log[4], (unsigned)STORE_FLAG); // the promise comes last
}

TEST("the payload and the flag both arrive") {
  struct mailbox mb = {0};
  bus_reset();

  mailbox_post(&mb, (const uint8_t[]){1, 2, 3, 4});

  CHECK_EQ(mb.ready, 1u);
  CHECK_EQ(mb.payload[0], 1u);
  CHECK_EQ(mb.payload[3], 4u);
}
