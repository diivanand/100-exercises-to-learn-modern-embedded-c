// Solution -- 03.05 Two dimensions: strides, and pointers to arrays

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

uint8_t pixel_at(const uint8_t *fb, size_t cols, size_t r, size_t c) {
  // Row-major: a whole row of `cols` pixels lies between vertical
  // neighbours, one pixel between horizontal ones. The row index gets the
  // big stride.
  return fb[r * cols + c];
}

// `const uint8_t (*rows)[3]` is a pointer to ARRAYS OF THREE: rows[r] is a
// whole row, rows[r][c] a pixel, and ++ on it moves three bytes. This is
// what a 2-D array argument actually decays to -- NOT uint8_t**, which
// would be a table of row POINTERS scattered anywhere in memory. The two
// have different memory layouts and are not interchangeable; passing one
// where the other is expected does not compile, which is the type system
// doing you a favour.
unsigned row_sum(const uint8_t (*rows)[3], size_t nrows, size_t r) {
  (void)nrows; // callers with a bounds bug would want an assert here (09.05)
  unsigned sum = 0;
  for (size_t c = 0; c < 3; ++c) {
    sum += rows[r][c];
  }
  return sum;
}

TEST("flat indexing walks row-major") {
  // A 3x3 tile, values chosen so every position is distinct.
  static const uint8_t tile[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  CHECK_EQ(pixel_at(tile, 3, 0, 0), 1u);
  CHECK_EQ(pixel_at(tile, 3, 1, 2), 6u); // row 1, col 2 -- not col 1, row 2
  CHECK_EQ(pixel_at(tile, 3, 2, 1), 8u);
}

TEST("flat indexing on a non-square buffer") {
  // 2 rows x 4 cols: on a non-square buffer, a swapped stride is not just
  // a transposed answer -- it indexes off the end.
  static const uint8_t wide[8] = {10, 11, 12, 13, 20, 21, 22, 23};
  CHECK_EQ(pixel_at(wide, 4, 0, 3), 13u);
  CHECK_EQ(pixel_at(wide, 4, 1, 1), 21u);
}

TEST("a pointer-to-array parameter takes real rows") {
  static const uint8_t img[2][3] = {{1, 2, 3}, {10, 20, 30}};
  CHECK_EQ(row_sum(img, 2, 0), 6u);
  CHECK_EQ(row_sum(img, 2, 1), 60u);
}
