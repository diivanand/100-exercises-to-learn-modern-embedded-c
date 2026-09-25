// Solution -- 10.05 The mock: when the ORDER is the contract

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// --- flash bus addresses and keys ------------------------------------------------

#define FLASH_CTRL_ADDR 0x40022004u
#define FLASH_STATUS_ADDR 0x40022008u
#define FLASH_UNLOCK_KEY 0xA5u
#define FLASH_LOCK_KEY 0x5Au
#define FLASH_STATUS_BUSY 0x1u

// --- the io seam -------------------------------------------------------------------

struct flash_io {
  void (*write)(uint32_t addr, uint32_t value);
  uint32_t (*read)(uint32_t addr);
};

// --- flash driver under test --------------------------------------------------------

void flash_program(const struct flash_io *io, uint32_t addr, uint32_t value) {
  io->write(FLASH_CTRL_ADDR, FLASH_UNLOCK_KEY);
  io->write(addr, value);
  // Wait for the part to finish before locking: locking mid-program aborts
  // the write on real silicon. The mock's script decides how long "busy"
  // lasts, so this loop is bounded by the test, not by luck.
  while ((io->read(FLASH_STATUS_ADDR) & FLASH_STATUS_BUSY) != 0u) {
  }
  io->write(FLASH_CTRL_ADDR, FLASH_LOCK_KEY);
}

// --- tests: a mock that scripts the bus ----------------------------------------------

enum mock_op { MOCK_WRITE, MOCK_READ };

struct expectation {
  enum mock_op op;
  uint32_t addr;
  uint32_t value; // for WRITE: the value that must be written
                  // for READ: the canned value to hand back
};

enum { MOCK_MAX_EXPECTATIONS = 16 };
static struct expectation mock_script[MOCK_MAX_EXPECTATIONS];
static size_t mock_scripted;
static size_t mock_cursor;

static void mock_reset(void) {
  mock_scripted = 0;
  mock_cursor = 0;
}

static void mock_expect_write(uint32_t addr, uint32_t value) {
  REQUIRE(mock_scripted < MOCK_MAX_EXPECTATIONS);
  mock_script[mock_scripted++] =
      (struct expectation){.op = MOCK_WRITE, .addr = addr, .value = value};
}

static void mock_expect_read(uint32_t addr, uint32_t canned) {
  REQUIRE(mock_scripted < MOCK_MAX_EXPECTATIONS);
  mock_script[mock_scripted++] =
      (struct expectation){.op = MOCK_READ, .addr = addr, .value = canned};
}

// The two functions below ARE the mock: each call must match the next line
// of the script, or the test fails on the spot. REQUIRE (not CHECK) keeps a
// runaway driver from marching past the end of the script.
static void mock_write(uint32_t addr, uint32_t value) {
  REQUIRE(mock_cursor < mock_scripted);
  const struct expectation *e = &mock_script[mock_cursor++];
  CHECK_EQ((int)e->op, (int)MOCK_WRITE);
  CHECK_EQ(e->addr, addr);
  CHECK_EQ(e->value, value);
}

static uint32_t mock_read(uint32_t addr) {
  REQUIRE(mock_cursor < mock_scripted);
  const struct expectation *e = &mock_script[mock_cursor++];
  CHECK_EQ((int)e->op, (int)MOCK_READ);
  CHECK_EQ(e->addr, addr);
  return e->value;
}

static void mock_verify_complete(void) {
  CHECK_EQ(mock_cursor, mock_scripted); // everything scripted must have happened
}

static const struct flash_io mock_io = {mock_write, mock_read};

TEST("programming a word: unlock, program, wait until ready, lock") {
  mock_reset();
  mock_expect_write(FLASH_CTRL_ADDR, FLASH_UNLOCK_KEY);
  mock_expect_write(0x08010000u, 0xCAFED00Du);
  mock_expect_read(FLASH_STATUS_ADDR, FLASH_STATUS_BUSY); // still programming
  mock_expect_read(FLASH_STATUS_ADDR, FLASH_STATUS_BUSY); // still programming
  mock_expect_read(FLASH_STATUS_ADDR, 0u);                // done
  mock_expect_write(FLASH_CTRL_ADDR, FLASH_LOCK_KEY);

  flash_program(&mock_io, 0x08010000u, 0xCAFED00Du);
  mock_verify_complete();
}

TEST("an instantly-ready part is read once and locked") {
  mock_reset();
  mock_expect_write(FLASH_CTRL_ADDR, FLASH_UNLOCK_KEY);
  mock_expect_write(0x08010100u, 0x00000001u);
  mock_expect_read(FLASH_STATUS_ADDR, 0u);
  mock_expect_write(FLASH_CTRL_ADDR, FLASH_LOCK_KEY);

  flash_program(&mock_io, 0x08010100u, 0x00000001u);
  mock_verify_complete();
}
