// =============================================================================
//  14.02 -- A frame parser that trusts nothing
// =============================================================================
//
//  A UART hands you bytes. Not frames, not messages: bytes, one at a time,
//  starting wherever the wire happened to be when you powered up, with
//  noise mixed in. Between that stream and anything meaningful stands a
//  parser, and this is the course's flagship exercise because everything
//  converges here: the state machine discipline of 13.01, the bounded
//  buffers of chapter 08, the checked lengths of chapter 04, and 14.01's
//  CRC (included above the parser as given code).
//
//  THE WIRE FORMAT, precisely:
//
//      offset  size      meaning
//      0       1         SOF, always 0xAA ("start of frame")
//      1       1         LEN: payload bytes only, 0..32
//      2       1         CMD
//      3       LEN       payload
//      3+LEN   2         CRC-16/CCITT-FALSE over LEN, CMD and payload,
//                        big-endian (high byte first)
//
//  THE PARSER'S CONTRACT. `parser_feed` takes ONE byte and returns at most
//  one event: EVENT_FRAME (a whole, CRC-clean frame is in `p->frame`),
//  EVENT_ERROR (something was wrong; counted, dropped), or EVENT_NONE.
//  It never blocks, never allocates, never reads ahead -- it is the shape
//  of code that lives in or next to an interrupt handler (12.07).
//
//  Three rules make such a parser survivable in the field:
//
//   1. VALIDATE LEN BEFORE BUFFERING A SINGLE BYTE. The length field is
//      corruption-controlled input. Unchecked, a flipped bit in LEN turns
//      into a write past the end of `payload` -- the classic embedded
//      remote-crash. (Chapter 04's lesson, at protocol scale.)
//
//   2. EVERY EXIT RE-ARMS THE HUNT. After a frame -- good OR bad -- the
//      next state is "hunting for SOF". The starter's error path instead
//      leaves the parser expecting a length byte, so the SOF of the NEXT
//      frame is swallowed as a length and one line error cascades into two
//      lost frames. Real parsers have shipped with exactly this bug; the
//      back-to-back-after-error test is written for it.
//
//   3. SAY LITTLE ABOUT GARBAGE. While hunting, non-SOF bytes are dropped
//      silently -- reporting an error per noise byte would flood whatever
//      is listening. Errors are for things that LOOKED like a frame.
//
//  TASK
//    Two bugs, both marked TODO: the missing LEN check and the broken
//    error-path resync. Fix both. Do not change the tests.
//
//  RUN IT
//    ./mec test 14_02
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// --- given: CRC-16/CCITT-FALSE, built in 14.01 --------------------------------

#define CRC16_INIT 0xFFFFu

static uint16_t crc16_step(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)((uint16_t)byte << 8);
  for (int bit = 0; bit < 8; ++bit) {
    crc = (crc & 0x8000u) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                          : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint16_t crc16_buf(const uint8_t *data, size_t len) {
  uint16_t crc = CRC16_INIT;
  for (size_t i = 0; i < len; ++i) {
    crc = crc16_step(crc, data[i]);
  }
  return crc;
}

// --- the parser ----------------------------------------------------------------

enum { FRAME_SOF = 0xAA, FRAME_MAX_PAYLOAD = 32 };

enum frame_event { EVENT_NONE, EVENT_FRAME, EVENT_ERROR };

struct frame {
  uint8_t cmd;
  uint8_t len;
  uint8_t payload[FRAME_MAX_PAYLOAD];
};

enum parser_state { PS_HUNT_SOF, PS_LEN, PS_CMD, PS_PAYLOAD, PS_CRC_HI, PS_CRC_LO };

struct frame_parser {
  enum parser_state state;
  struct frame frame;
  uint8_t payload_got;
  uint16_t crc;      // running CRC over LEN, CMD, payload
  uint8_t crc_hi;    // first received CRC byte, parked until the second
  uint32_t crc_errors;
};

void parser_init(struct frame_parser *p) {
  memset(p, 0, sizeof *p);
  p->state = PS_HUNT_SOF;
}

enum frame_event parser_feed(struct frame_parser *p, uint8_t byte) {
  switch (p->state) {
  case PS_HUNT_SOF:
    if (byte == FRAME_SOF) {
      p->state = PS_LEN;
    }
    return EVENT_NONE; // everything else is line noise; say nothing

  case PS_LEN:
    // TODO: rule 1. This accepts LEN = 255 and will happily buffer 255
    // bytes into a 32-byte array.
    p->frame.len = byte;
    p->payload_got = 0;
    p->crc = crc16_step(CRC16_INIT, byte);
    p->state = PS_CMD;
    return EVENT_NONE;

  case PS_CMD:
    p->frame.cmd = byte;
    p->crc = crc16_step(p->crc, byte);
    p->state = (p->frame.len > 0) ? PS_PAYLOAD : PS_CRC_HI;
    return EVENT_NONE;

  case PS_PAYLOAD:
    p->frame.payload[p->payload_got] = byte;
    ++p->payload_got;
    p->crc = crc16_step(p->crc, byte);
    if (p->payload_got == p->frame.len) {
      p->state = PS_CRC_HI;
    }
    return EVENT_NONE;

  case PS_CRC_HI:
    p->crc_hi = byte;
    p->state = PS_CRC_LO;
    return EVENT_NONE;

  case PS_CRC_LO: {
    const uint16_t received = (uint16_t)(((uint16_t)p->crc_hi << 8) | byte);
    if (received != p->crc) {
      ++p->crc_errors;
      // TODO: rule 2. "We were mid-frame, so the next byte is surely a
      // fresh length" -- no. This state eats the next frame's SOF.
      p->state = PS_LEN;
      return EVENT_ERROR;
    }
    p->state = PS_HUNT_SOF;
    return EVENT_FRAME;
  }
  }
  return EVENT_NONE; // unreachable: every state is handled above
}

// --- tests ----------------------------------------------------------------------

// Feeds a byte string, counting what comes out.
static void run(struct frame_parser *p, const uint8_t *bytes, size_t n,
                unsigned *frames, unsigned *errors) {
  for (size_t i = 0; i < n; ++i) {
    switch (parser_feed(p, bytes[i])) {
    case EVENT_FRAME:
      ++*frames;
      break;
    case EVENT_ERROR:
      ++*errors;
      break;
    case EVENT_NONE:
      break;
    }
  }
}

// CRC over {02 10 DE AD} = 0x78A4, verified against 14.01's oracle.
static const uint8_t golden[] = {0xAA, 0x02, 0x10, 0xDE, 0xAD, 0x78, 0xA4};

TEST("a clean frame parses, and not a byte earlier") {
  struct frame_parser p;
  parser_init(&p);
  for (size_t i = 0; i + 1 < sizeof golden; ++i) {
    CHECK_EQ(parser_feed(&p, golden[i]), EVENT_NONE);
  }
  CHECK_EQ(parser_feed(&p, golden[sizeof golden - 1]), EVENT_FRAME);
  CHECK_EQ(p.frame.cmd, 0x10u);
  CHECK_EQ(p.frame.len, 2u);
  CHECK_EQ(p.frame.payload[0], 0xDEu);
  CHECK_EQ(p.frame.payload[1], 0xADu);
}

TEST("a zero-payload frame parses") {
  // CRC over {00 01} = 0x0D2E, oracle-verified.
  const uint8_t ping[] = {0xAA, 0x00, 0x01, 0x0D, 0x2E};
  struct frame_parser p;
  parser_init(&p);
  unsigned frames = 0, errors = 0;
  run(&p, ping, sizeof ping, &frames, &errors);
  CHECK_EQ(frames, 1u);
  CHECK_EQ(errors, 0u);
  CHECK_EQ(p.frame.len, 0u);
}

TEST("garbage before a frame is skipped in silence") {
  struct frame_parser p;
  parser_init(&p);
  unsigned frames = 0, errors = 0;
  const uint8_t noise[] = {0x13, 0x37, 0x00, 0xFF};
  run(&p, noise, sizeof noise, &frames, &errors);
  CHECK_EQ(frames, 0u);
  CHECK_EQ(errors, 0u);
  run(&p, golden, sizeof golden, &frames, &errors);
  CHECK_EQ(frames, 1u);
}

TEST("a corrupted frame is dropped, counted, and does not eat its successor") {
  struct frame_parser p;
  parser_init(&p);
  uint8_t bad[sizeof golden];
  memcpy(bad, golden, sizeof golden);
  bad[3] ^= 0x01u; // one flipped payload bit, as lines do

  unsigned frames = 0, errors = 0;
  run(&p, bad, sizeof bad, &frames, &errors);
  CHECK_EQ(frames, 0u);
  CHECK_EQ(errors, 1u);
  CHECK_EQ(p.crc_errors, 1u);

  // The very next frame must parse. A parser that is still waiting for
  // "the rest" of the dead frame eats this one's SOF -- the resync bug.
  run(&p, golden, sizeof golden, &frames, &errors);
  CHECK_EQ(frames, 1u);
  CHECK_EQ(errors, 1u);
}

TEST("an impossible length is rejected before it buffers anything") {
  struct frame_parser p;
  parser_init(&p);
  CHECK_EQ(parser_feed(&p, 0xAA), EVENT_NONE);
  CHECK_EQ(parser_feed(&p, 0xFF), EVENT_ERROR); // LEN 255 > 32: reject NOW

  // And the parser is immediately usable again.
  unsigned frames = 0, errors = 0;
  run(&p, golden, sizeof golden, &frames, &errors);
  CHECK_EQ(frames, 1u);
}

TEST("a maximum-length frame is still legal") {
  uint8_t wire[3 + FRAME_MAX_PAYLOAD + 2];
  wire[0] = 0xAA;
  wire[1] = FRAME_MAX_PAYLOAD;
  wire[2] = 0x42;
  for (size_t i = 0; i < FRAME_MAX_PAYLOAD; ++i) {
    wire[3 + i] = (uint8_t)i;
  }
  const uint16_t crc = crc16_buf(&wire[1], 2 + FRAME_MAX_PAYLOAD);
  wire[3 + FRAME_MAX_PAYLOAD] = (uint8_t)(crc >> 8);
  wire[4 + FRAME_MAX_PAYLOAD] = (uint8_t)(crc & 0xFFu);

  struct frame_parser p;
  parser_init(&p);
  unsigned frames = 0, errors = 0;
  run(&p, wire, sizeof wire, &frames, &errors);
  CHECK_EQ(frames, 1u);
  CHECK_EQ(errors, 0u);
  CHECK_EQ(p.frame.len, (unsigned)FRAME_MAX_PAYLOAD);
  CHECK_EQ(p.frame.payload[31], 31u);
}

TEST("back-to-back frames both parse") {
  uint8_t two[sizeof golden * 2];
  memcpy(two, golden, sizeof golden);
  memcpy(two + sizeof golden, golden, sizeof golden);

  struct frame_parser p;
  parser_init(&p);
  unsigned frames = 0, errors = 0;
  run(&p, two, sizeof two, &frames, &errors);
  CHECK_EQ(frames, 2u);
  CHECK_EQ(errors, 0u);
}
