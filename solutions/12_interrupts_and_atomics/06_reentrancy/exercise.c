// Solution -- 12.06 Reentrancy: no hidden statics in shared code paths

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// The fault-injection point (given): format_hex calls this, if set, at its
// most vulnerable moment. The tests use it to play an interrupt.
void (*format_hex_test_hook)(void);

// --- the code under test ------------------------------------------------------

char *format_hex(uint32_t value, char *buf, size_t size) {
  if (buf == NULL || size < 9u) {
    return NULL;
  }
  // Straight into the CALLER'S buffer. No object in this function outlives
  // the call or is visible outside it, so a second activation -- from an
  // ISR, or from the hook below -- works on its own storage and cannot
  // touch ours. That property, not any keyword, is what "reentrant" means.
  for (int i = 7; i >= 0; --i) {
    buf[i] = "0123456789ABCDEF"[value & 0xFu];
    value >>= 4;
  }
  buf[8] = '\0';
  if (format_hex_test_hook != NULL) {
    format_hex_test_hook(); // an "interrupt" fires; we no longer care
  }
  return buf;
}

// --- the tests ------------------------------------------------------------------

TEST("formats eight zero-padded uppercase digits") {
  char buf[9];
  CHECK_EQ(format_hex(0x1A2B3C4Du, buf, sizeof buf), "1A2B3C4D");
  CHECK_EQ(format_hex(0u, buf, sizeof buf), "00000000");
  CHECK_EQ(format_hex(0xFFFFFFFFu, buf, sizeof buf), "FFFFFFFF");
  CHECK(format_hex(5u, buf, 8u) == NULL); // too small for 8 digits + NUL
}

static char g_nested[9];

static void interrupt_formats_too(void) {
  format_hex_test_hook = NULL; // one level of preemption is enough
  (void)format_hex(0x00C0FFEEu, g_nested, sizeof g_nested);
}

TEST("surviving an interrupt that also formats") {
  char buf[9];
  format_hex_test_hook = interrupt_formats_too;
  const char *result = format_hex(0xDEADBEEFu, buf, sizeof buf);
  format_hex_test_hook = NULL;
  CHECK_EQ(result, "DEADBEEF");
  CHECK_EQ((const char *)g_nested, "00C0FFEE"); // the "ISR"'s own result
}
