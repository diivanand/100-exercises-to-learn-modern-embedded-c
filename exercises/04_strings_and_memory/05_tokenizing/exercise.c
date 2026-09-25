// =============================================================================
//  04.05 -- Tokenizing without strtok
// =============================================================================
//
//  "SET LED 1" arrives over the UART; the firmware needs three words. The
//  library offers strtok, and strtok earns its ban in three counts:
//
//   1. HIDDEN STATIC STATE. strtok(NULL, ...) means "continue where the
//      LAST call left off" -- last call anywhere in the program. One parse
//      at a time, ever. Two modules parsing, or a parse touched from an
//      interrupt, and they silently corrupt each other (CERT CON33-C; the
//      reentrancy chapter, 12.06, makes this concrete).
//
//   2. IT WRITES ON YOUR INPUT. strtok terminates each token by smashing a
//      NUL over the separator. The line must be writable -- so a command
//      table in flash cannot be tokenized at all -- and after parsing, the
//      original line is gone (goodbye, error message quoting the input).
//
//   3. THE ESCAPE HATCHES ARE NOT STANDARD. strtok_r is POSIX; strtok_s is
//      Annex K (see 04.02 for how that went). Neither is ISO C that you
//      can count on from every vendor toolchain.
//
//  The embedded answer costs a dozen lines and no compromise: SLICES. A
//  token is a (pointer, length) pair looking into the caller's line --
//  no copy, no allocation, no writes, no state. The pattern is everywhere
//  once you see it: it is C++'s string_view and Rust's &str, done by hand.
//  One rule comes with it: a slice is a VIEW, valid only as long as the
//  line it points into (chapter 03's lifetime discipline).
//
//  The starter "works" -- once. It copies the line into a static scratch
//  buffer and hands out pointers into THAT. Tokenize a second line and
//  every slice from the first is quietly rewritten. The third test is
//  exactly that scenario; watch it fail before you fix it.
//
//  TASK
//    Rewrite `tokenize` as a stateless, non-writing slice walk (strspn and
//    strcspn help, or walk the indices by hand). Do not change the tests.
//
//  RUN IT
//    ./mec test 04_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// A slice: a view into somebody else's bytes. No copy, no allocation, no
// terminator of its own -- just where it starts and how long it is.
struct token {
  const char *start;
  size_t len;
};

// Split `line` on spaces into at most max_tokens slices; return the count.
size_t tokenize(const char *line, struct token out[], size_t max_tokens) {
  // TODO: a static scratch copy plus strtok's own static cursor. The
  // slices point into `scratch`, so the NEXT call rewrites them all.
  static char scratch[64];
  snprintf(scratch, sizeof scratch, "%s", line);
  size_t n = 0;
  for (char *t = strtok(scratch, " "); t != NULL && n < max_tokens;
       t = strtok(NULL, " ")) {
    out[n].start = t;
    out[n].len = strlen(t);
    ++n;
  }
  return n;
}

// Test helper: does a slice spell this word?
static bool token_is(const struct token *t, const char *word) {
  return t->len == strlen(word) && memcmp(t->start, word, t->len) == 0;
}

TEST("a command line splits into words") {
  struct token tok[4];
  const size_t n = tokenize("SET LED 1", tok, 4);
  CHECK_EQ(n, 3u);
  CHECK(token_is(&tok[0], "SET"));
  CHECK(token_is(&tok[1], "LED"));
  CHECK(token_is(&tok[2], "1"));
}

TEST("repeated separators and empty lines") {
  struct token tok[4];
  const size_t n = tokenize("  GET   TEMP  ", tok, 4);
  CHECK_EQ(n, 2u);
  CHECK(token_is(&tok[0], "GET"));
  CHECK(token_is(&tok[1], "TEMP"));
  CHECK_EQ(tokenize("", tok, 4), 0u);
  CHECK_EQ(tokenize("   ", tok, 4), 0u);
}

TEST("two lines tokenized in turn do not share memory") {
  struct token a[4];
  struct token b[4];
  const size_t na = tokenize("SET LED 1", a, 4);
  const size_t nb = tokenize("GET TEMP", b, 4); // must not disturb a[]
  CHECK_EQ(na, 3u);
  CHECK_EQ(nb, 2u);
  CHECK(token_is(&a[0], "SET")); // a[] must still read the FIRST line
  CHECK(token_is(&a[2], "1"));
  CHECK(token_is(&b[1], "TEMP"));
}

TEST("more words than slots is clamped, not overrun") {
  struct token tok[2];
  CHECK_EQ(tokenize("A B C D", tok, 2), 2u);
}
