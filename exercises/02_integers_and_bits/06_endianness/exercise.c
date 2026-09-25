// =============================================================================
//  02.06 -- Byte order is a property of storage, not of values
// =============================================================================
//
//  The value 0x1234 has no byte order. STORED, it does: a little-endian CPU
//  puts 0x34 at the lower address, a big-endian one puts 0x12 there. Your
//  Mac and the STM32L476 are little-endian; classic network order and many
//  sensor protocols are big-endian; USB and most of Intel's world are
//  little-endian. (Effective C ch. 8, "Endian".)
//
//  A wire format is a contract about BYTES AT OFFSETS. The tempting way to
//  parse one is to declare "the same" struct and copy the bytes over it:
//
//      struct sensor_reading r;
//      memcpy(&r, frame, sizeof r);
//
//  This is wrong THREE separate ways, and the tests catch two of them today:
//
//   1. FIELD BYTE ORDER. The wire says big-endian; your struct fields are
//      read in the CPU's order. Every multi-byte field comes out mirrored:
//      device id 0x1234 parses as 0x3412.
//
//   2. PADDING. The compiler inserts invisible bytes to align members (a
//      uint32_t after a uint16_t gets 2 bytes of padding before it -- 05.01
//      is all about this). `sizeof r` here is 12, the frame is 8: fields
//      land at the wrong offsets AND the copy reads 4 bytes past the frame.
//
//   3. IT ONLY EVER WORKED BY LUCK. Change compiler, ABI, or a #pragma pack
//      setting somewhere upstream and the layout moves. The wire format,
//      meanwhile, has not changed at all.
//
//  The portable parser is boring, explicit and unbreakable: read bytes at
//  offsets and build values with shifts -- exactly the `read_be16` /
//  `read_be32` you wrote in 02.01. It compiles to a load-and-swap on every
//  serious compiler; you pay nothing for the portability.
//
//  One wrinkle is worth learning now: the temperature is a SIGNED 16-bit
//  field. `read_be16` hands you 0xFF38 as an unsigned 65336; converting
//  that to int16_t is implementation-defined in C17 (6.3.1.3p3). The
//  defined spelling is: if it is above INT16_MAX, subtract 65536 in wider
//  arithmetic first. (C23 finally mandates two's-complement and blesses the
//  plain cast; your MISRA checker does not care and wants the defined form.)
//
//  TASK
//    Rewrite `parse_reading` to read the frame field by field, big-endian,
//    with a correct signed conversion. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_06
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct sensor_reading {
  uint16_t device_id;
  uint32_t timestamp;
  int16_t temp_centi; // hundredths of a degree, signed
};

// The wire format says: 8 bytes, big-endian, no padding anywhere --
//   [0..1] device id   [2..5] timestamp   [6..7] temperature
void parse_reading(const uint8_t frame[8], struct sensor_reading *out) {
  // TODO: mirrored fields, misplaced offsets, and a read past the frame.
  memcpy(out, frame, sizeof *out);
}

TEST("a frame parses field by field") {
  //                          id           timestamp               temp
  const uint8_t frame[8] = {0x12, 0x34, 0x00, 0x01, 0xE2, 0x40, 0x00, 0xFA};
  struct sensor_reading r;
  parse_reading(frame, &r);
  CHECK_EQ(r.device_id, 0x1234u);
  CHECK_EQ(r.timestamp, 123456u); // 0x0001E240
  CHECK_EQ(r.temp_centi, 250);    // +2.50 degrees
}

TEST("negative temperatures survive the trip") {
  const uint8_t frame[8] = {0xBE, 0xEF, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x38};
  struct sensor_reading r;
  parse_reading(frame, &r);
  CHECK_EQ(r.device_id, 0xBEEFu);
  CHECK_EQ(r.timestamp, 1u);
  CHECK_EQ(r.temp_centi, -200); // 0xFF38: -2.00 degrees
}
