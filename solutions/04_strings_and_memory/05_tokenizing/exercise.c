// Solution -- 04.05 Tokenizing without strtok

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// A slice: a view into somebody else's bytes. No copy, no allocation, no
// terminator of its own -- just where it starts and how long it is.
struct token {
  const char *start;
  size_t len;
};

size_t tokenize(const char *line, struct token out[], size_t max_tokens) {
  // A plain index walk: skip separators, mark a start, run to the end of
  // the word. No hidden state -- everything this function knows is in its
  // parameters -- and no writes: `line` can live in flash.
  size_t n = 0;
  const char *p = line;
  while (n < max_tokens) {
    while (*p == ' ') {
      ++p;
    }
    if (*p == '\0') {
      break;
    }
    const char *start = p;
    while (*p != ' ' && *p != '\0') {
      ++p;
    }
    out[n].start = start;
    out[n].len = (size_t)(p - start);
    ++n;
  }
  return n;
  // The strspn/strcspn spelling of the same walk, for the curious:
  //   p += strspn(p, " ");             len = strcspn(p, " ");
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
