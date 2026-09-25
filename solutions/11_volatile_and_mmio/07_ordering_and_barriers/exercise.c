// Solution -- 11.07 Volatile orders volatile; everything else needs a fence

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
  uint8_t payload[4];    // ordinary memory
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
  // Payload first -- the flag is a PROMISE that the payload is in place.
  for (size_t i = 0; i < 4; ++i) {
    store_payload_byte(mb, i, data[i]);
  }
  // The compiler fence: no ordinary store above this line may be moved
  // below it. The volatile store to `ready` alone would not hold the plain
  // payload stores in place -- volatile only orders volatile.
  atomic_signal_fence(memory_order_release);
  store_ready(mb);
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
