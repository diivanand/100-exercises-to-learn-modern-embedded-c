// =============================================================================
//  07.04 -- Conditional compilation without the folklore
// =============================================================================
//
//  Embedded code bases live and die by #if: one source tree, many boards,
//  many feature sets. The mechanics are simple (Effective C ch. 9,
//  "Conditional Inclusion"); the discipline is what this exercise is about.
//
//  KNOW YOUR THREE FORMS.
//
//      #ifdef CFG_X        // is the name defined AT ALL? value irrelevant
//      #if defined(CFG_X)  // same test, but composable with && and ||
//      #if CFG_X           // is the name defined to a NONZERO value?
//
//  The difference is a classic footgun: configure a build with -DCFG_X=0 to
//  turn a feature OFF, and every `#ifdef CFG_X` in the tree still says ON --
//  the name is defined; its value never mattered. Prefer 0/1-valued flags
//  tested with `#if`, so that "off" and "unset" can at least be told apart.
//
//  AND THERE IS A WORSE ONE, waiting in this very file. If the name in the
//  directive is MISSPELT, `#ifdef CFG_FRAME_CHECKSUN` is not an error -- an
//  unknown name is simply "not defined" (and under `#if` it silently
//  evaluates as 0). The feature quietly compiles out, the linker is happy,
//  and nothing notices until a peer device rejects your frames. GCC's
//  -Wundef catches bare undefined names under #if; this course's warning
//  set does not include it, and #ifdef is beyond even -Wundef's help.
//
//  The robust pattern puts a TRIPWIRE at the top of the file: demand that
//  the configuration be defined, explicitly, on pain of #error --
//
//      #if !defined(CFG_FRAME_CHECKSUM)
//      #error "CFG_FRAME_CHECKSUM must be defined to 0 or 1"
//      #endif
//
//  Now a typo in the -D flag, a stale build script, or a renamed option is
//  a build break with a message, not a silent behaviour change. MISRA C
//  requires exactly this discipline (every identifier in a preprocessor
//  condition defined before use).
//
//  Two side notes worth keeping. To disable a REGION of code, `#if 0` beats
//  comment markers: it nests, and it survives code that itself contains
//  comments. And when you find yourself writing the same #if ladder in ten
//  functions, stop: hide the variation behind one small interface instead.
//  Chapter 10 makes that move properly (10.02).
//
//  TASK
//    The build (the #define at the top of the file stands in for it) asked
//    for CFG_FRAME_CHECKSUM=1, yet the tests say the feature is off. Find
//    the misspelling, fix it, and add the #error tripwire so the next typo
//    is a build error instead of a quiet lie.
//
//  RUN IT
//    ./mec test 07_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Stands in for the build system's -DCFG_FRAME_CHECKSUM=1.
#define CFG_FRAME_CHECKSUM 1

// TODO: add the tripwire here -- and read the directives below suspiciously.

static bool checksum_enabled(void) {
#ifdef CFG_FRAME_CHECKSUN
  return true;
#else
  return false;
#endif
}

// Encode a payload for the wire; with the checksum feature on, append the
// XOR of the payload bytes.
static size_t frame_encode(const uint8_t *payload, size_t len, uint8_t *out) {
  for (size_t i = 0; i < len; ++i) {
    out[i] = payload[i];
  }
#ifdef CFG_FRAME_CHECKSUN
  uint8_t x = 0;
  for (size_t i = 0; i < len; ++i) {
    x ^= payload[i];
  }
  out[len] = x;
  return len + 1;
#else
  return len;
#endif
}

TEST("the feature the build asked for is actually compiled in") {
  CHECK(checksum_enabled());
}

TEST("an encoded frame carries its checksum byte") {
  const uint8_t payload[] = {0x10, 0x20, 0x33};
  uint8_t out[8];
  CHECK_EQ(frame_encode(payload, sizeof payload, out), (size_t)4);
  CHECK_EQ(out[0], 0x10u);
  CHECK_EQ(out[3], 0x03u); // 0x10 ^ 0x20 ^ 0x33
}

TEST("a one-byte payload checksums to itself") {
  const uint8_t payload[] = {0xAB};
  uint8_t out[4];
  CHECK_EQ(frame_encode(payload, sizeof payload, out), (size_t)2);
  CHECK_EQ(out[1], 0xABu);
}
