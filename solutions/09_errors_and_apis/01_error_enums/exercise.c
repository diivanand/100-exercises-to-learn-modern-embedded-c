// Solution -- 09.01 One status enum, and every failure keeps its name

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

enum flash_status {
  FLASH_OK = 0, // 0 = success, so `if (flash_erase_page(p))` reads as "if it failed"
  FLASH_ERR_BAD_ARG,
  FLASH_ERR_BUSY,
  FLASH_ERR_TIMEOUT,
};

#define FLASH_PAGE_COUNT 256u

// --- fake hardware, driven by the tests --------------------------------------

static bool fake_busy;  // another operation already in progress
static bool fake_stuck; // the ready flag never comes back
static uint32_t last_erased_page = UINT32_MAX;

static void fake_reset(void) {
  fake_busy = false;
  fake_stuck = false;
  last_erased_page = UINT32_MAX;
}

static bool wait_ready(void) {
  for (int i = 0; i < 1000; ++i) { // bounded, always (11.05)
    if (!fake_stuck) {
      return true;
    }
  }
  return false;
}

// --- the driver ---------------------------------------------------------------

enum flash_status flash_erase_page(uint32_t page) {
  // Each failure keeps its identity all the way to the caller. The caller's
  // recovery differs per cause -- BAD_ARG is a bug to log, BUSY means retry
  // later, TIMEOUT means the part may be wearing out -- so collapsing them
  // into one bit throws away exactly the information recovery needs.
  if (page >= FLASH_PAGE_COUNT) {
    return FLASH_ERR_BAD_ARG;
  }
  if (fake_busy) {
    return FLASH_ERR_BUSY;
  }
  if (!wait_ready()) {
    return FLASH_ERR_TIMEOUT;
  }
  last_erased_page = page;
  return FLASH_OK;
}

const char *flash_status_name(enum flash_status st) {
  // Designated initialisers keep the table in lockstep with the enum
  // (01.04); the bounds guard keeps a corrupted status from walking off the
  // end (04.07).
  static const char *const names[] = {
      [FLASH_OK] = "ok",
      [FLASH_ERR_BAD_ARG] = "bad argument",
      [FLASH_ERR_BUSY] = "busy",
      [FLASH_ERR_TIMEOUT] = "timeout",
  };
  if (st < FLASH_OK || st > FLASH_ERR_TIMEOUT) {
    return "?";
  }
  return names[st];
}

TEST("a good erase succeeds and reports OK") {
  fake_reset();
  CHECK_EQ(flash_erase_page(7), FLASH_OK);
  CHECK_EQ(last_erased_page, 7u);
}

TEST("each failure keeps its name") {
  fake_reset();
  CHECK_EQ(flash_erase_page(FLASH_PAGE_COUNT), FLASH_ERR_BAD_ARG);

  fake_reset();
  fake_busy = true;
  CHECK_EQ(flash_erase_page(7), FLASH_ERR_BUSY);

  fake_reset();
  fake_stuck = true;
  CHECK_EQ(flash_erase_page(7), FLASH_ERR_TIMEOUT);
  CHECK_EQ(last_erased_page, UINT32_MAX); // a timed-out erase erased nothing
}

TEST("status names for the log") {
  CHECK_EQ(flash_status_name(FLASH_OK), "ok");
  CHECK_EQ(flash_status_name(FLASH_ERR_TIMEOUT), "timeout");
  CHECK_EQ(flash_status_name((enum flash_status)99), "?");
}
