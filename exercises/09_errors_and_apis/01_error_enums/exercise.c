// =============================================================================
//  09.01 -- One status enum, and every failure keeps its name
// =============================================================================
//
//  C has no exceptions. Nothing unwinds, nothing propagates on its own: if a
//  function can fail, its return value says so, and the CALLER decides what
//  happens next. That is not a poverty of the language -- it is the whole
//  design. Firmware people like that every failure path is visible in the
//  source, because every failure path has to be TESTED.
//
//  The discipline this chapter builds, piece by piece:
//
//   - one small STATUS ENUM per subsystem, with 0 meaning success, so
//     `if (err)` reads naturally and a zeroed struct defaults to "fine";
//   - every fallible function RETURNS it (results travel through pointer
//     parameters -- 09.02);
//   - failures keep their identity. The caller's recovery differs per
//     cause: BAD_ARG is a bug to log, BUSY means try again later, TIMEOUT
//     means the hardware may be failing. A bool collapses all three into
//     "no", and the caller is left guessing.
//
//  (The other C convention you will meet is the kernel's negative-errno
//  style -- 0 for success, -EINVAL and friends for failure, which packs
//  status and small results into one int. Fine style; this course uses
//  enums because they name the failure AND give the debugger and -Wswitch
//  something to hold on to. Effective C ch. 11 covers the assert side of
//  this boundary; CERT ERR33-C is "detect and handle" -- which requires
//  errors distinguishable enough to handle.)
//
//  THE DRIVER BELOW was ported from a code base where everything returned
//  bool, and it shows:
//
//   - a busy device and a bad page number come back as the SAME code;
//   - worse, a TIMEOUT comes back as FLASH_OK -- the classic swapped
//     convention bug when bool-thinking (`true` = good) meets status-enum
//     thinking (`0` = good, and 0 is `false`...).
//
//  TASK
//    Make every failure return its own code, and only success return
//    FLASH_OK. Then look at `flash_status_name`: the log needs a name for a
//    status even when the status is garbage. Do not change the tests.
//
//  RUN IT
//    ./mec test 09_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

enum flash_status {
  FLASH_OK = 0,
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
  // TODO: this still thinks in bool. Distinguish the failures -- and check
  // what a timeout actually returns.
  bool ok = true;
  if (page >= FLASH_PAGE_COUNT) {
    ok = false;
  }
  if (fake_busy) {
    ok = false;
  }
  if (!wait_ready()) {
    return FLASH_OK; // "false means failure" met "0 means success"
  }
  if (ok) {
    last_erased_page = page;
  }
  return ok ? FLASH_OK : FLASH_ERR_BAD_ARG;
}

const char *flash_status_name(enum flash_status st) {
  static const char *const names[] = {
      [FLASH_OK] = "ok",
      [FLASH_ERR_BAD_ARG] = "bad argument",
      [FLASH_ERR_BUSY] = "busy",
      [FLASH_ERR_TIMEOUT] = "timeout",
  };
  // TODO: a corrupted status walks straight off the end of this table.
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
