// =============================================================================
//  04.02 -- Copying strings without lying about it
// =============================================================================
//
//  strcpy copies until it finds the NUL, full stop. If the destination is
//  too small, that is your problem -- CERT STR31-C again, and half the
//  CVE database with it. So everyone reaches for strncpy, "the safe one".
//
//  It is not. strncpy was designed in the 1970s for fixed-width directory
//  entries in the Unix filesystem, and it has two behaviours that make
//  sense there and nowhere else (Effective C ch. 7 tells the story):
//
//   - If src is SHORTER than n, it zero-fills the rest of the field --
//     harmless here, wasted cycles on a big buffer.
//   - If src is LONGER than or equal to n, it copies n bytes and writes
//     NO TERMINATOR AT ALL. The "safe" copy quietly manufactured the very
//     thing CERT STR32-C exists to forbid: a char array that looks like a
//     string and is not one. The next strlen or printf on that field walks
//     off the end.
//
//  What embedded code actually wants when a name meets a fixed field:
//  TRUNCATE AND TERMINATE, and say that truncation happened so the caller
//  can decide whether it matters. Those are strlcpy's semantics -- BSD
//  invention, shipped by newlib too, but still not ISO C -- and they are
//  four lines to write portably. (The other portable spelling is
//  snprintf(dst, size, "%s", src), which also terminates and whose return
//  value reveals truncation; 04.03 leans on that contract hard.)
//
//  And the standard's own answer? C11 Annex K: strcpy_s, strncpy_s, with
//  runtime constraint handlers. Optional -- and glibc, musl and newlib all
//  declined to ship it. Code written against Annex K is code that builds
//  on one vendor's toolchain. Treat it as a dead end.
//
//  TASK
//    Rewrite `field_set` so it always terminates, truncates when it must,
//    reports truncation, and survives a zero-sized destination. Do not
//    change the tests.
//
//  RUN IT
//    ./mec test 04_02      (and re-run under: cmake --preset asan)
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Copy src into a field of dst_size bytes: always NUL-terminated,
// truncated if it must be. Returns true if the WHOLE of src fitted.
bool field_set(char *dst, size_t dst_size, const char *src) {
  // TODO: "the safe one" neither terminates on truncation nor reports it.
  strncpy(dst, src, dst_size);
  return strlen(src) < dst_size;
}

TEST("a short name copies and terminates") {
  char field[8];
  memset(field, 0xAB, sizeof field); // dirty, like a reused record
  CHECK(field_set(field, sizeof field, "OK"));
  CHECK_EQ(field, "OK");
}

TEST("a long name truncates AND terminates") {
  char field[8];
  memset(field, 0xAB, sizeof field);
  CHECK_FALSE(field_set(field, sizeof field, "TEMPERATURE"));
  CHECK_EQ(strlen(field), 7u); // without the terminator this walks off
  CHECK_EQ(field, "TEMPERA");
}

TEST("an exact fit is still a fit") {
  char field[8];
  memset(field, 0xAB, sizeof field);
  CHECK(field_set(field, sizeof field, "PRESSUR")); // 7 chars + NUL = 8
  CHECK_EQ(field, "PRESSUR");
}

TEST("a zero-sized field is refused outright") {
  CHECK_FALSE(field_set(NULL, 0, "X"));
}
