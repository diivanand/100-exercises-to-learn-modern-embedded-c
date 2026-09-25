// =============================================================================
//  03.05 -- Two dimensions: strides, and pointers to arrays
// =============================================================================
//
//  Embedded C meets two-dimensional data constantly -- frame buffers, sensor
//  grids, lookup surfaces -- and stores almost all of it the same way: one
//  flat allocation, ROW-MAJOR. Row r, column c lives at
//
//      fb[r * cols + c]
//
//  because C lays out `uint8_t img[ROWS][COLS]` as row 0's COLS bytes, then
//  row 1's, and so on. The row index is the one that gets multiplied by the
//  stride. Swap the roles and on a square buffer you read the transposed
//  pixel -- annoying; on a non-square buffer you index PAST THE END --
//  undefined (CERT ARR30-C again). The starter has done the swap; the tests
//  catch it both ways.
//
//  When a real 2-D array is passed to a function, it decays ONE level: an
//  `uint8_t img[2][3]` argument becomes a POINTER TO ITS ROWS,
//
//      const uint8_t (*rows)[3]     // pointer to arrays-of-3
//
//  on which the compiler still knows the stride: rows[1] is three bytes past
//  rows[0], and rows[r][c] does the row-major arithmetic for you. Note what
//  it is NOT: `const uint8_t **` -- a pointer to an array of POINTERS, a
//  different memory layout entirely (each row could live anywhere). The two
//  spellings do not convert to each other, and the day you try to pass
//  `img` to a `uint8_t **` parameter the compiler will refuse. Believe it.
//
//  Which spelling should an API use? The flat (pointer, cols) pair when the
//  dimensions vary at run time -- that is most real frame buffers, and it is
//  what `pixel_at` does. The pointer-to-array form when a dimension is fixed
//  by the problem -- `row_sum` below takes rows of exactly 3, say an RGB
//  triple per sample, and gets bounds-checked column arithmetic for free.
//
//  TASK
//    Fix the stride in `pixel_at`; implement `row_sum`. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 03_05
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

uint8_t pixel_at(const uint8_t *fb, size_t cols, size_t r, size_t c) {
  // TODO: the stride is on the wrong index.
  return fb[c * cols + r];
}

// Sum of one row, where a row is exactly three bytes (an RGB sample, say).
unsigned row_sum(const uint8_t (*rows)[3], size_t nrows, size_t r) {
  // TODO: rows[r] is a whole row; rows[r][c] is one byte of it.
  (void)rows;
  (void)nrows;
  (void)r;
  return 0;
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
