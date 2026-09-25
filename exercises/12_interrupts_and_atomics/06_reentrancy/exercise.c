// =============================================================================
//  12.06 -- Reentrancy: no hidden statics in shared code paths
// =============================================================================
//
//  A function is REENTRANT when a second activation can begin before the
//  first has finished -- an ISR calling what main was in the middle of --
//  and both still work. The property is structural: no mutable static or
//  global state, no heap shared behind the caller's back, nothing held
//  between the call's first instruction and its last that another
//  activation could clobber.
//
//  You met the disease in 04.05: strtok keeps its position in a hidden
//  static, so two in-progress tokenisations destroy each other (CERT
//  CON33-C lists the libc offenders -- rand, strtok, localtime...; newlib,
//  the embedded libc, wraps errno and friends in a per-context `_reent`
//  struct precisely because of this). The shapes to recognise in review:
//
//   - the STATIC SCRATCH BUFFER: "formats into an internal buffer and
//     returns a pointer to it" (this exercise's starter; also 06.01's
//     returning-a-local cousin);
//   - the LAZY CACHE: "first call builds a table" -- two first calls, one
//     half-built table (06.05);
//   - the STATEFUL FILTER: static accumulator serving two channels (06.05
//     again -- the fix there, a context struct, is the fix here).
//
//  The interrupt in this exercise is SIMULATED, and deterministically so:
//  `format_hex` calls the function pointer `format_hex_test_hook` at its
//  most vulnerable moment, and the test installs a hook that does exactly
//  what a rude ISR would -- calls format_hex again. Fault injection at the
//  worst instant, every run, no dice rolled. (Grenning builds test seams
//  like this throughout TDD for Embedded C; chapter 10 taught the pattern.)
//
//  The starter "modernised" this function's signature -- callers now pass
//  a buffer -- but kept the old static workshop inside, formatting there
//  and copying out. Single-threaded tests pass. The nested activation
//  overwrites the workshop between the format and the copy, and the caller
//  receives the INTERRUPT'S value, correctly NUL-terminated, plausible,
//  and wrong. This is what reentrancy bugs look like in the field: not
//  crashes -- convincing garbage.
//
//  TASK
//    Make `format_hex` genuinely reentrant: format directly into the
//    caller's buffer and delete the static. Keep calling the hook at the
//    same point -- a reentrant function has nothing to fear from it. Do
//    not change the tests.
//
//  RUN IT
//    ./mec test 12_06
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The fault-injection point (given): format_hex calls this, if set, at its
// most vulnerable moment. The tests use it to play an interrupt.
void (*format_hex_test_hook)(void);

// --- the code under test ------------------------------------------------------

char *format_hex(uint32_t value, char *buf, size_t size) {
  if (buf == NULL || size < 9u) {
    return NULL;
  }
  // TODO: the "workshop" static below is shared by every activation of
  // this function. The hook -- standing in for an ISR -- runs while our
  // result is still in the workshop, not yet copied out.
  static char scratch[9];
  for (int i = 7; i >= 0; --i) {
    scratch[i] = "0123456789ABCDEF"[value & 0xFu];
    value >>= 4;
  }
  scratch[8] = '\0';
  if (format_hex_test_hook != NULL) {
    format_hex_test_hook(); // an "interrupt" fires here
  }
  memcpy(buf, scratch, 9u);
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
