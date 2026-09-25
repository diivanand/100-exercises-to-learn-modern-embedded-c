// Solution -- 14.04 The command station

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ============================ given code =====================================
// Condensed, final versions of the pieces built in 14.01-14.03 and 08.06.

#define CRC16_INIT 0xFFFFu

static uint16_t crc16_step(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)((uint16_t)byte << 8);
  for (int bit = 0; bit < 8; ++bit) {
    crc = (crc & 0x8000u) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                          : (uint16_t)(crc << 1);
  }
  return crc;
}

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
  uint16_t crc;
  uint8_t crc_hi;
  uint32_t crc_errors;
};

static void parser_init(struct frame_parser *p) {
  memset(p, 0, sizeof *p);
  p->state = PS_HUNT_SOF;
}

static enum frame_event parser_feed(struct frame_parser *p, uint8_t byte) {
  switch (p->state) {
  case PS_HUNT_SOF:
    if (byte == FRAME_SOF) {
      p->state = PS_LEN;
    }
    return EVENT_NONE;
  case PS_LEN:
    if (byte > FRAME_MAX_PAYLOAD) {
      p->state = PS_HUNT_SOF;
      return EVENT_ERROR;
    }
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
    p->frame.payload[p->payload_got++] = byte;
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
    p->state = PS_HUNT_SOF;
    if (received != p->crc) {
      ++p->crc_errors;
      return EVENT_ERROR;
    }
    return EVENT_FRAME;
  }
  }
  return EVENT_NONE;
}

static int32_t q16_mv_from_adc(uint16_t raw) {
  return (int32_t)(((int64_t)raw * 3300 << 16) / 4095);
}

static int32_t ema_step(int32_t y, int32_t x, uint32_t alpha_q16) {
  const int64_t scaled = (int64_t)alpha_q16 * ((int64_t)x - y);
  return y + (int32_t)((scaled + 32768) >> 16);
}

static int16_t q16_to_mv_i16(int32_t q16) {
  const int32_t mv = (int32_t)(((int64_t)q16 + 32768) >> 16);
  if (mv > INT16_MAX) {
    return INT16_MAX;
  }
  if (mv < INT16_MIN) {
    return INT16_MIN;
  }
  return (int16_t)mv;
}

enum { OUT_CAP = 128 }; // power of two (08.06)

struct ring {
  uint8_t buf[OUT_CAP];
  uint32_t head, tail;
};

static void ring_put(struct ring *r, uint8_t b) {
  if (r->head - r->tail == OUT_CAP) {
    return; // full: drop -- the tests drain long before this
  }
  r->buf[r->head & (OUT_CAP - 1u)] = b;
  ++r->head;
}

static bool ring_take(struct ring *r, uint8_t *b) {
  if (r->head == r->tail) {
    return false;
  }
  *b = r->buf[r->tail & (OUT_CAP - 1u)];
  ++r->tail;
  return true;
}

// ============================ the station ====================================

enum {
  CMD_PING = 0x01,
  CMD_READ_SENSOR = 0x02,
  CMD_SET_RATE = 0x03,
  CMD_GET_STATS = 0x04,
  CMD_COUNT // one past the last real command: the table's size
};
#define RSP_FLAG 0x80u
#define CMD_NAK 0x7Fu
#define FILTER_ALPHA 16384u // 0.25 in Q0.16

// One static instance: init/reset via station_init (09.07). On the board
// this module is fed by the UART ISR; here, by the tests.
static struct station {
  struct frame_parser parser;
  struct ring out;
  int32_t filter_y;
  uint8_t rate;
  uint16_t unknown_cmds;
} st;

static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len) {
  ring_put(&st.out, FRAME_SOF);
  ring_put(&st.out, len);
  ring_put(&st.out, cmd);
  uint16_t crc = crc16_step(CRC16_INIT, len);
  crc = crc16_step(crc, cmd);
  for (uint8_t i = 0; i < len; ++i) {
    ring_put(&st.out, payload[i]);
    crc = crc16_step(crc, payload[i]);
  }
  ring_put(&st.out, (uint8_t)(crc >> 8));
  ring_put(&st.out, (uint8_t)(crc & 0xFFu));
}

static void send_nak(uint8_t offending_cmd) {
  send_frame(CMD_NAK, &offending_cmd, 1);
}

static void handle_ping(const struct frame *f) {
  (void)f;
  send_frame(CMD_PING | RSP_FLAG, NULL, 0);
}

static void handle_read_sensor(const struct frame *f) {
  (void)f;
  const uint16_t mv = (uint16_t)q16_to_mv_i16(st.filter_y);
  const uint8_t payload[2] = {(uint8_t)(mv >> 8), (uint8_t)(mv & 0xFFu)};
  send_frame(CMD_READ_SENSOR | RSP_FLAG, payload, sizeof payload);
}

static void handle_set_rate(const struct frame *f) {
  if (f->len != 1) {
    send_nak(f->cmd);
    return;
  }
  st.rate = f->payload[0];
  send_frame(CMD_SET_RATE | RSP_FLAG, f->payload, 1);
}

static void handle_get_stats(const struct frame *f) {
  (void)f;
  const uint16_t crc_errs = (uint16_t)(st.parser.crc_errors & 0xFFFFu);
  const uint8_t payload[4] = {
      (uint8_t)(crc_errs >> 8), (uint8_t)(crc_errs & 0xFFu),
      (uint8_t)(st.unknown_cmds >> 8), (uint8_t)(st.unknown_cmds & 0xFFu)};
  send_frame(CMD_GET_STATS | RSP_FLAG, payload, sizeof payload);
}

typedef void (*cmd_handler)(const struct frame *f);

static const cmd_handler handlers[CMD_COUNT] = {
    [CMD_PING] = handle_ping,
    [CMD_READ_SENSOR] = handle_read_sensor,
    [CMD_SET_RATE] = handle_set_rate,
    [CMD_GET_STATS] = handle_get_stats,
};

static void dispatch(const struct frame *f) {
  // The command byte came off a wire: it indexes OUR table only after WE
  // have checked it. Slot [0] is deliberately NULL, hence the second test.
  if (f->cmd >= CMD_COUNT || handlers[f->cmd] == NULL) {
    ++st.unknown_cmds;
    send_nak(f->cmd);
    return;
  }
  handlers[f->cmd](f);
}

void station_init(void) {
  memset(&st, 0, sizeof st);
  parser_init(&st.parser);
  st.rate = 10;
}

void station_adc_sample(uint16_t raw) {
  st.filter_y = ema_step(st.filter_y, q16_mv_from_adc(raw), FILTER_ALPHA);
}

void station_feed(uint8_t byte) {
  if (parser_feed(&st.parser, byte) == EVENT_FRAME) {
    dispatch(&st.parser.frame);
  }
}

size_t station_take_output(uint8_t *dst, size_t cap) {
  size_t n = 0;
  while (n < cap && ring_take(&st.out, &dst[n])) {
    ++n;
  }
  return n;
}

// ============================ tests ==========================================

static void feed_bytes(const uint8_t *bytes, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    station_feed(bytes[i]);
  }
}

// Every expected reply below was cross-checked against the 14.01 bitwise
// oracle; the CRC noted in each comment is over LEN, CMD, payload.

TEST("PING is answered byte-for-byte") {
  station_init();
  const uint8_t req[] = {0xAA, 0x00, 0x01, 0x0D, 0x2E}; // CRC(00 01)=0x0D2E
  feed_bytes(req, sizeof req);

  uint8_t out[16];
  const size_t n = station_take_output(out, sizeof out);
  const uint8_t expect[] = {0xAA, 0x00, 0x81, 0x9C, 0xA6}; // CRC(00 81)=0x9CA6
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}

TEST("READ_SENSOR reports the settled filter value") {
  station_init();
  for (int i = 0; i < 200; ++i) {
    station_adc_sample(1241); // 1000.07 mV; settles to 1000 on the wire
  }
  const uint8_t req[] = {0xAA, 0x00, 0x02, 0x3D, 0x4D}; // CRC(00 02)=0x3D4D
  feed_bytes(req, sizeof req);

  uint8_t out[16];
  const size_t n = station_take_output(out, sizeof out);
  // payload 0x03E8 = 1000 mV; CRC(02 82 03 E8) = 0x15E7
  const uint8_t expect[] = {0xAA, 0x02, 0x82, 0x03, 0xE8, 0x15, 0xE7};
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}

TEST("SET_RATE echoes its payload back") {
  station_init();
  const uint8_t req[] = {0xAA, 0x01, 0x03, 0x0A, 0x0F, 0xB5}; // CRC=0x0FB5
  feed_bytes(req, sizeof req);

  uint8_t out[16];
  const size_t n = station_take_output(out, sizeof out);
  const uint8_t expect[] = {0xAA, 0x01, 0x83, 0x0A, 0x14, 0x2D}; // CRC=0x142D
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}

TEST("an unknown command is NAKed, never dispatched") {
  station_init();
  const uint8_t req[] = {0xAA, 0x00, 0x55, 0x17, 0x5F}; // CRC(00 55)=0x175F
  feed_bytes(req, sizeof req);

  uint8_t out[16];
  const size_t n = station_take_output(out, sizeof out);
  // NAK carries the offending command; CRC(01 7F 55) = 0xE99B
  const uint8_t expect[] = {0xAA, 0x01, 0x7F, 0x55, 0xE9, 0x9B};
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}

TEST("GET_STATS counts CRC errors and unknown commands") {
  station_init();
  const uint8_t bad_ping[] = {0xAA, 0x00, 0x01, 0x0D, 0x2F}; // CRC off by one
  feed_bytes(bad_ping, sizeof bad_ping);
  const uint8_t unknown[] = {0xAA, 0x00, 0x55, 0x17, 0x5F};
  feed_bytes(unknown, sizeof unknown);

  uint8_t drain[16];
  (void)station_take_output(drain, sizeof drain); // discard the NAK

  const uint8_t req[] = {0xAA, 0x00, 0x04, 0x5D, 0x8B}; // CRC(00 04)=0x5D8B
  feed_bytes(req, sizeof req);

  uint8_t out[16];
  const size_t n = station_take_output(out, sizeof out);
  // one CRC error, one unknown; CRC(04 84 00 01 00 01) = 0x8476
  const uint8_t expect[] = {0xAA, 0x04, 0x84, 0x00, 0x01, 0x00, 0x01, 0x84, 0x76};
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}

TEST("noise between commands costs nothing") {
  station_init();
  const uint8_t ping[] = {0xAA, 0x00, 0x01, 0x0D, 0x2E};
  const uint8_t noise[] = {0x13, 0x37, 0xFF};

  feed_bytes(noise, sizeof noise);
  feed_bytes(ping, sizeof ping);
  feed_bytes(noise, sizeof noise);
  feed_bytes(ping, sizeof ping);

  uint8_t out[32];
  const size_t n = station_take_output(out, sizeof out);
  const uint8_t expect[] = {0xAA, 0x00, 0x81, 0x9C, 0xA6,
                            0xAA, 0x00, 0x81, 0x9C, 0xA6};
  CHECK_EQ(n, sizeof expect);
  CHECK_MEM_EQ(out, expect, sizeof expect);
}
