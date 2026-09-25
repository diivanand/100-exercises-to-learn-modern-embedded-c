// Solution -- 02.08 Alignment: the constraint you cannot see in the source

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t read_u32be(const uint8_t *p) {
  // Byte reads have no alignment requirement, and the shifts build the
  // value in arithmetic -- one function solves the alignment problem and
  // the byte-order problem at once (02.01, 02.06).
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

struct accel_sample {
  int16_t x, y, z;
};

struct accel_sample read_sample(const uint8_t *p) {
  // Native-order data at an arbitrary offset: memcpy into a properly
  // aligned local is THE blessed idiom. The compiler knows this pattern
  // and compiles it to plain loads where the target allows them -- there
  // is no performance penalty for writing it correctly.
  struct accel_sample s;
  memcpy(&s, p, sizeof s);
  return s;
}

// A descriptor the (imaginary) DMA engine reads: it requires the buffer to
// start on a 4-byte boundary.
struct dma_slot {
  uint8_t busy;
  _Alignas(4) uint8_t buf[8]; // _Alignas: the requirement, in the code
};

_Static_assert(offsetof(struct dma_slot, buf) % 4 == 0,
               "DMA buffer must be 4-byte aligned");

TEST("big-endian words at odd offsets") {
  //             hdr   [ u32be @ 1  .......]  [ u32be @ 5 ........]
  const uint8_t stream[] = {0xAA, 0x12, 0x34, 0x56, 0x78, 0xDE, 0xAD, 0xBE, 0xEF};
  CHECK_EQ(read_u32be(stream + 1), 0x12345678u);
  CHECK_EQ(read_u32be(stream + 5), 0xDEADBEEFu);
}

TEST("accelerometer samples at odd offsets") {
  // One status byte, then x=100, y=-2, z=513, little-endian (native) order.
  const uint8_t stream[] = {0x01, 0x64, 0x00, 0xFE, 0xFF, 0x01, 0x02};
  const struct accel_sample s = read_sample(stream + 1);
  CHECK_EQ(s.x, 100);
  CHECK_EQ(s.y, -2);
  CHECK_EQ(s.z, 513);
}

TEST("the DMA buffer really is aligned") {
  CHECK_EQ(offsetof(struct dma_slot, buf) % 4, 0u);
  static struct dma_slot slot;
  CHECK_EQ((uintptr_t)slot.buf % 4, 0u);
}
