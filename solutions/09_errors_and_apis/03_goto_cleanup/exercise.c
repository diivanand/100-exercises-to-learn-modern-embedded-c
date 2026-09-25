// Solution -- 09.03 goto cleanup: unwinding by hand, in reverse, once

#include <mect/mect.h>

#include <stdbool.h>

enum xfer_status {
  XFER_OK = 0,
  XFER_ERR_NO_CHANNEL,
  XFER_ERR_NO_BUFFER,
  XFER_ERR_NO_LOCK,
};

// --- three tiny resource pools ------------------------------------------------
//
// Counters instead of real DMA channels, but the accounting is the real
// thing: `outstanding` is what a leak looks like in numbers, and the tests
// read it. `fail_next` lets a test inject failure at any acquisition site --
// every failure path gets exercised, which is the whole point of writing
// them explicitly.

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
static void chan_release(void) { --chan_outstanding; }

static bool buf_acquire(void) {
  if (buf_fail_next) {
    buf_fail_next = false;
    return false;
  }
  ++buf_outstanding;
  return true;
}
static void buf_release(void) { --buf_outstanding; }

static bool lock_acquire(void) {
  if (lock_fail_next) {
    lock_fail_next = false;
    return false;
  }
  ++lock_outstanding;
  return true;
}
static void lock_release(void) { --lock_outstanding; }

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
  // The goto chain (CERT MEM12-C). Acquisitions run top to bottom; releases
  // run bottom to top; each failure jumps to the label that releases
  // exactly what is already held. Adding a fourth resource touches two
  // lines. The nested-if version this replaced touched every branch.
  enum xfer_status st;

  if (!chan_acquire()) {
    st = XFER_ERR_NO_CHANNEL;
    goto out;
  }
  if (!buf_acquire()) {
    st = XFER_ERR_NO_BUFFER;
    goto release_chan;
  }
  if (!lock_acquire()) {
    st = XFER_ERR_NO_LOCK;
    goto release_buf;
  }

  ++transfers_done; // the actual work, with everything held
  st = XFER_OK;

  lock_release();
release_buf:
  buf_release();
release_chan:
  chan_release();
out:
  return st;
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
