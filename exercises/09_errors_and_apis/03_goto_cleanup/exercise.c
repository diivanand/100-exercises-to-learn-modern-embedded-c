// =============================================================================
//  09.03 -- goto cleanup: unwinding by hand, in reverse, once
// =============================================================================
//
//  A function acquires three resources, and the second acquisition fails.
//  Who releases the first? In C++ destructors do it; in C, YOU are the
//  destructor. The language gives you exactly one tool that makes the
//  unwinding read top-to-bottom without duplicating a release into every
//  branch: `goto`, aimed strictly forward, at a chain of labels that
//  release in REVERSE order of acquisition:
//
//      if (!chan_acquire())  { st = ...; goto out; }
//      if (!buf_acquire())   { st = ...; goto release_chan; }
//      if (!lock_acquire())  { st = ...; goto release_buf; }
//      ...the work...
//      st = XFER_OK;
//      lock_release();
//    release_buf:
//      buf_release();
//    release_chan:
//      chan_release();
//    out:
//      return st;
//
//  Note the success path FALLS THROUGH the same labels: one copy of every
//  release, one return, and adding a fourth resource touches two lines.
//  This is the Linux kernel's house pattern, and CERT MEM12-C recommends it
//  by name ("Consider using a goto chain when leaving a function on error").
//  It is not the goto Dijkstra wrote about: it only jumps forward, only to
//  cleanup, and it REPLACES the control-flow spaghetti of nested ifs rather
//  than creating it.
//
//  THE STARTER is the nested-if version -- the one everybody writes first.
//  Its lock-failure branch releases the channel and forgets the buffer.
//  Note the shape of the bug: not wrong logic, MISSING logic, in the branch
//  that runs least often. The last test hammers the failure paths a hundred
//  times, because that is what a leak looks like in the field: fine on the
//  bench, dead after a weekend of retries. (The pools here are counters;
//  `total_outstanding()` is the leak detector. Chapter 08 built real ones.)
//
//  TASK
//    Rewrite `run_transfer` as a goto chain. Every failure path must
//    release exactly what was acquired, in reverse order. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 09_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>

enum xfer_status {
  XFER_OK = 0,
  XFER_ERR_NO_CHANNEL,
  XFER_ERR_NO_BUFFER,
  XFER_ERR_NO_LOCK,
};

// --- three tiny resource pools ------------------------------------------------

static int chan_outstanding, buf_outstanding, lock_outstanding;
static bool chan_fail_next, buf_fail_next, lock_fail_next;

static bool chan_acquire(void) {
  if (chan_fail_next) {
    chan_fail_next = false;
    return false;
  }
  ++chan_outstanding;
  return true;
}
static void chan_release(void) {
  --chan_outstanding;
}

static bool buf_acquire(void) {
  if (buf_fail_next) {
    buf_fail_next = false;
    return false;
  }
  ++buf_outstanding;
  return true;
}
static void buf_release(void) {
  --buf_outstanding;
}

static bool lock_acquire(void) {
  if (lock_fail_next) {
    lock_fail_next = false;
    return false;
  }
  ++lock_outstanding;
  return true;
}
static void lock_release(void) {
  --lock_outstanding;
}

static int transfers_done;

static void pools_reset(void) {
  chan_outstanding = buf_outstanding = lock_outstanding = 0;
  chan_fail_next = buf_fail_next = lock_fail_next = false;
  transfers_done = 0;
}

static int total_outstanding(void) {
  return chan_outstanding + buf_outstanding + lock_outstanding;
}

// --- the function under test ---------------------------------------------------

enum xfer_status run_transfer(void) {
  // TODO: rewrite as a goto chain. Before you do, find the leak by eye --
  // it is in the branch that runs least often.
  if (chan_acquire()) {
    if (buf_acquire()) {
      if (lock_acquire()) {
        ++transfers_done;
        lock_release();
        buf_release();
        chan_release();
        return XFER_OK;
      }
      chan_release();
      return XFER_ERR_NO_LOCK;
    }
    chan_release();
    return XFER_ERR_NO_BUFFER;
  }
  return XFER_ERR_NO_CHANNEL;
}

TEST("the happy path acquires, works, and returns everything") {
  pools_reset();
  CHECK_EQ(run_transfer(), XFER_OK);
  CHECK_EQ(transfers_done, 1);
  CHECK_EQ(total_outstanding(), 0);
}

TEST("failure at the first acquisition leaks nothing") {
  pools_reset();
  chan_fail_next = true;
  CHECK_EQ(run_transfer(), XFER_ERR_NO_CHANNEL);
  CHECK_EQ(transfers_done, 0);
  CHECK_EQ(total_outstanding(), 0);
}

TEST("failure in the middle releases the channel") {
  pools_reset();
  buf_fail_next = true;
  CHECK_EQ(run_transfer(), XFER_ERR_NO_BUFFER);
  CHECK_EQ(total_outstanding(), 0);
}

TEST("failure at the last acquisition releases buffer AND channel") {
  pools_reset();
  lock_fail_next = true;
  CHECK_EQ(run_transfer(), XFER_ERR_NO_LOCK);
  CHECK_EQ(transfers_done, 0);
  CHECK_EQ(total_outstanding(), 0); // the starter leaks the buffer here
}

TEST("a leak would compound: run every failure repeatedly") {
  pools_reset();
  for (int i = 0; i < 100; ++i) {
    chan_fail_next = true;
    (void)run_transfer();
    buf_fail_next = true;
    (void)run_transfer();
    lock_fail_next = true;
    (void)run_transfer();
  }
  CHECK_EQ(total_outstanding(), 0); // 100 leaked buffers is a dead board
}
