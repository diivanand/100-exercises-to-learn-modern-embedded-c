// =============================================================================
//  02.08 -- Alignment: the constraint you cannot see in the source
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR. -Wcast-align is doing its
//  job; read on.
//
//  Every type has an ALIGNMENT: an address multiple it must live at.
//  uint32_t aligns to 4; a struct aligns to its strictest member. `_Alignof`
//  asks for it, `_Alignas` demands it. Byte buffers align to 1 -- which is
//  the problem, because parsing means finding wider values at arbitrary
//  offsets inside byte buffers.
//
//  The tempting parse is a pointer cast:
//
//      uint32_t v = *(const uint32_t *)(buf + 5);     // three bugs in one
//
//   1. UNDEFINED BEHAVIOUR if buf+5 is not 4-aligned (C17 6.3.2.3p7; CERT
//      EXP36-C). What actually happens is the worst kind of lottery: a
//      Cortex-M0 hard-faults; a Cortex-M4 quietly performs most unaligned
//      loads (slower, and never for LDM/LDRD/exclusive accesses -- so the
//      same code faults depending on what the optimiser picked); your Mac
//      does not care at all. Code that "worked for years" on the bench
//      faults in the field when the compiler chooses a different
//      instruction.
//
//   2. BYTE ORDER (02.06): even where the load works, it reads the CPU's
//      order, not the wire's.
//
//   3. STRICT ALIASING: a uint8_t buffer accessed as uint32_t is also an
//      aliasing violation the optimiser may punish independently.
//
//  This build makes the cast itself an error (-Wcast-align), which is why
//  the starter does not compile. The fixes are mechanical:
//
//   - Wire-order fields: read BYTES and build the value with shifts
//     (02.01/02.06). Solves alignment and byte order in one stroke.
//   - Native-order blobs: `memcpy` into a properly-typed, properly-aligned
//     local. The compiler recognises the idiom and emits plain loads where
//     the target allows -- correctness costs nothing.
//
//  And when YOU are the one laying out memory that hardware will read -- a
//  DMA engine that requires 4-byte-aligned buffers, say -- state the
//  requirement in the type with `_Alignas`, and pin it with a
//  `_Static_assert(offsetof(...))` so a refactor cannot silently break it.
//  The starter's `dma_slot` puts its buffer at offset 1; the test can see.
//
//  TASK
//    Rewrite `read_u32be` (shifts) and `read_sample` (memcpy), and align
//    `dma_slot.buf`. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_08
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

uint32_t read_u32be(const uint8_t *p) {
  // TODO: unaligned, aliasing, wrong byte order -- and it does not compile.
  return *(const uint32_t *)p;
}

struct accel_sample {
  int16_t x, y, z;
};

struct accel_sample read_sample(const uint8_t *p) {
  // TODO: same cast, one struct up.
  return *(const struct accel_sample *)p;
}

// A descriptor the (imaginary) DMA engine reads: it requires the buffer to
// start on a 4-byte boundary.
struct dma_slot {
  uint8_t busy;
  uint8_t buf[8]; // TODO: at offset 1, the hardware will not read this
};

TEST("big-endian words at odd offsets") {
  //             hdr   [ u32be @ 1  .......]  [ u32be @ 5 ........]
  const uint8_t stream[] = {0xAA, 0x12, 0x34, 0x56, 0x78, 0xDE, 0xAD,
                            0xBE, 0xEF};
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
