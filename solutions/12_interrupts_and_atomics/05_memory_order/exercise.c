// Solution -- 12.05 Memory order: release what you wrote, acquire what they did

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the code under test ------------------------------------------------------

enum { MAILBOX_WORDS = 256 }; // a 1 KB telemetry block, not a toy pair

static uint32_t g_box[MAILBOX_WORDS];
static _Atomic bool g_ready;

void mailbox_send(const uint32_t payload[MAILBOX_WORDS]) {
  // Wait until the previous message has been taken. acquire pairs with the
  // receiver's release-store of `false`: once we see the box free, its
  // READS of the old payload are complete and we may overwrite it.
  while (atomic_load_explicit(&g_ready, memory_order_acquire)) {
    // a real ISR would count a drop instead of spinning (12.07)
  }
  for (int i = 0; i < MAILBOX_WORDS; ++i) {
    g_box[i] = payload[i];
  }
  // Fill first, THEN release the flag: everything written above is visible
  // to whoever acquires `true`.
  atomic_store_explicit(&g_ready, true, memory_order_release);
}

bool mailbox_try_receive(uint32_t out[MAILBOX_WORDS]) {
  if (!atomic_load_explicit(&g_ready, memory_order_acquire)) {
    return false;
  }
  // Copied high to low. In a correctly synchronised program the direction
  // of this loop CANNOT matter -- that is what the release/acquire pair
  // buys. In the broken starter it decides everything: the last word the
  // writer touches is the first one read. There is no benign data race.
  for (int i = MAILBOX_WORDS - 1; i >= 0; --i) {
    out[i] = g_box[i];
  }
  atomic_store_explicit(&g_ready, false, memory_order_release);
  return true;
}

// --- the tests ------------------------------------------------------------------

TEST("empty until sent, then the payload, then empty again") {
  static uint32_t msg[MAILBOX_WORDS];
  CHECK_FALSE(mailbox_try_receive(msg));
  static uint32_t payload[MAILBOX_WORDS];
  for (uint32_t i = 0; i < MAILBOX_WORDS; ++i) {
    payload[i] = 7u + i;
  }
  mailbox_send(payload);
  REQUIRE(mailbox_try_receive(msg));
  CHECK_EQ(msg[0], 7u);
  CHECK_EQ(msg[MAILBOX_WORDS - 1], 7u + (MAILBOX_WORDS - 1u));
  CHECK_FALSE(mailbox_try_receive(msg));
}

enum { MESSAGES = 20000 };

static void *sender(void *arg) {
  (void)arg;
  for (uint32_t n = 0; n < (uint32_t)MESSAGES; ++n) {
    static uint32_t payload[MAILBOX_WORDS];
    for (uint32_t i = 0; i < MAILBOX_WORDS; ++i) {
      payload[i] = n + i; // word i of message n carries n + i
    }
    mailbox_send(payload);
  }
  return NULL;
}

TEST("a received message is never half old, half new") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, sender, NULL) == 0);

  uint32_t received = 0;
  uint32_t incoherent = 0;
  uint64_t patience = 0;
  while (received < (uint32_t)MESSAGES) {
    static uint32_t msg[MAILBOX_WORDS];
    if (!mailbox_try_receive(msg)) {
      if (++patience > 200000000ull) {
        break; // sender is stuck; the count check will say so
      }
      continue;
    }
    patience = 0;
    // Spot-check the block's coherence -- cheaply, so this loop gets back
    // to polling fast (a slow consumer would hide the bug by accident).
    // A torn fill is new words at the front, stale words at the back, so
    // the last word is the one that talks.
    if (msg[1] != msg[0] + 1u || msg[MAILBOX_WORDS / 2] != msg[0] + MAILBOX_WORDS / 2u ||
        msg[MAILBOX_WORDS - 1] != msg[0] + (MAILBOX_WORDS - 1u)) {
      ++incoherent;
    }
    ++received;
  }
  pthread_join(isr, NULL);
  CHECK_EQ(received, (uint32_t)MESSAGES);
  CHECK_EQ(incoherent, 0u);
}
