// =============================================================================
//  12.05 -- Memory order: release what you wrote, acquire what they did
// =============================================================================
//
//  12.04 used memory_order_release and _acquire in passing. They deserve
//  their own exercise, because "message passing" -- fill a payload, raise a
//  flag -- is the pattern under half of all lock-free code, and it fails in
//  two distinct ways:
//
//  1. PROGRAM ORDER IS WRONG. The starter raises the flag FIRST, then fills
//     the box -- "claim it, then write it". The receiver sees the flag,
//     reads mid-fill, and gets a payload that is half old, half new. No
//     exotic hardware needed; the source code says so.
//
//  2. PROGRAM ORDER IS RIGHT BUT UNENFORCED. Fix the source order and use
//     memory_order_relaxed, and you are still broken: relaxed promises
//     only that EACH atomic access is indivisible, nothing about the
//     ordering of the plain stores around it. An ARM core -- your Mac, and
//     your Cortex-M7 successor board -- may make the flag visible before
//     the payload. (An x86 machine mostly will not, which is how this bug
//     ships: it "passed" on the developer's desk. The weakly-ordered
//     machine you are sitting at is the better teacher.)
//
//  Notice the receiver below copies the block HIGH TO LOW. In a correct
//  program that is an irrelevant implementation detail -- and it must be:
//  if the direction of a loop can change your results, you do not have a
//  program, you have a race. Here it reads the writer's LAST word first,
//  which is why the starter fails on essentially every message instead of
//  one in a thousand. "It is only a benign race" is a sentence with no
//  referent; the standard calls every data race undefined (C17 5.1.2.4/35).
//
//  The contract that fixes both:
//
//     WRITER:  fill the box;  store the flag with RELEASE.
//              "Everything I wrote before this store is visible to
//               whoever acquires it."
//     READER:  load the flag with ACQUIRE; then read the box.
//              "If I saw the flag, I see everything released before it."
//
//  The pairing is the point: release without acquire (or either one
//  missing) promises nothing. `memory_order_seq_cst` -- what the plain
//  atomic_load/atomic_store spellings mean -- is release/acquire plus a
//  single global order; it is the correct DEFAULT, and relaxed is the
//  optimisation you justify in a comment, usually only for counters where
//  nothing else depends on the value (12.01's event count). C17 5.1.2.4
//  has the formal model; you need the two sentences above.
//
//  TASK
//    Fix `mailbox_send` and `mailbox_try_receive`: fill before flag, and a
//    proper release/acquire pair in each direction. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 12_05
//
// =============================================================================

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
  while (atomic_load_explicit(&g_ready, memory_order_relaxed)) {
    // a real ISR would count a drop instead of spinning (12.07)
  }
  // TODO: the flag goes up BEFORE the box is filled -- and relaxed order
  // would not save even the right sequence.
  atomic_store_explicit(&g_ready, true, memory_order_relaxed);
  for (int i = 0; i < MAILBOX_WORDS; ++i) {
    g_box[i] = payload[i];
  }
}

bool mailbox_try_receive(uint32_t out[MAILBOX_WORDS]) {
  if (!atomic_load_explicit(&g_ready, memory_order_relaxed)) {
    return false;
  }
  // Copied high to low. In a correctly synchronised program the direction
  // of this loop CANNOT matter -- that is what the release/acquire pair
  // buys. In the broken starter it decides everything: the last word the
  // writer touches is the first one read. There is no benign data race.
  for (int i = MAILBOX_WORDS - 1; i >= 0; --i) {
    out[i] = g_box[i];
  }
  atomic_store_explicit(&g_ready, false, memory_order_relaxed);
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
