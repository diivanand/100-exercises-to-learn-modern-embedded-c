// Solution -- 04.02 Copying strings without lying about it

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

bool field_set(char *dst, size_t dst_size, const char *src) {
  if (dst_size == 0) {
    return false; // no room even for the terminator
  }
  // An explicit loop with the two guarantees strncpy refuses to make:
  // always terminated, truncation reported. These are strlcpy's semantics
  // (BSD, and newlib has it) written out in ISO C.
  size_t i = 0;
  for (; i + 1 < dst_size && src[i] != '\0'; ++i) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
  return src[i] == '\0'; // false: we stopped because dst was full
  // The one-line alternative with the same guarantees:
  //   int n = snprintf(dst, dst_size, "%s", src);
  //   return n >= 0 && (size_t)n < dst_size;
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
