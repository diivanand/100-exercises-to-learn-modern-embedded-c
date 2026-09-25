// Solution -- 17.01 The frame link: bytes with a spine

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

// Not in bsp/l476_regs.h; defined here from the manual.
#define USART_ISR_ORE (1u << 3)   // overrun happened (RM0351 40.8.8)
#define USART_ICR_ORECF (1u << 3) // ...and its w1c clear bit (RM0351 40.8.9)

// --- given: tick ---------------------------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

static void prompt(const char *s) {
  while (*s != '\0') {
    bsp_uart_putc(*s++);
  }
  bsp_uart_putc('\r');
  bsp_uart_putc('\n');
}

// --- given: RX ring, filled by the USART2 interrupt (12.04 on real wires) ------

enum { RX_RING_SIZE = 64 }; // power of two: indices wrap by mask (08.06)

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head; // ISR writes
static volatile uint32_t rx_tail; // mainline writes
static volatile uint32_t rx_overruns;

void USART2_IRQHandler(void) {
  if ((USART2->ISR & USART_ISR_ORE) != 0u) { // a byte died in the shifter
    USART2->ICR = USART_ICR_ORECF;           // w1c, exactly one flag (11.04)
    ++rx_overruns;
  }
  while ((USART2->ISR & USART_ISR_RXNE) != 0u) {
    const uint8_t byte = (uint8_t)USART2->RDR; // reading clears RXNE
    if ((rx_head - rx_tail) < (uint32_t)RX_RING_SIZE) {
      rx_ring[rx_head % (uint32_t)RX_RING_SIZE] = byte;
      ++rx_head;
    }
    // A full ring drops the byte; the parser's CRC will catch the wound.
  }
}

static void uart_rx_init(void) {
  USART2->CR1 |= USART_CR1_RXNEIE;
  nvic_enable_irq(IRQN_USART2);
}

static bool rx_pop(uint8_t *out) {
  if (rx_tail == rx_head) {
    return false;
  }
  *out = rx_ring[rx_tail % (uint32_t)RX_RING_SIZE];
  ++rx_tail;
  return true;
}

// --- given: CRC-16/CCITT-FALSE (14.01) ------------------------------------------

static uint16_t crc16_step(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)((uint16_t)byte << 8);
  for (int b = 0; b < 8; ++b) {
    crc = (crc & 0x8000u) != 0u ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                                : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint16_t crc16(const uint8_t *data, uint32_t len) {
  uint16_t crc = 0xFFFFu;
  for (uint32_t i = 0; i < len; ++i) {
    crc = crc16_step(crc, data[i]);
  }
  return crc;
}

// --- the parser (yours) ----------------------------------------------------------

enum { FRAME_MAX_PAYLOAD = 32, FRAME_SOF = 0xAA };

enum rx_event {
  RX_NONE,    // byte consumed, nothing to report yet
  RX_FRAME,   // a complete frame with a good CRC is in *out
  RX_BAD_CRC, // a complete frame arrived, its CRC lied
  RX_BAD_LEN, // a LEN byte over the maximum; frame refused at the door
};

enum parser_state {
  HUNT_SOF,
  WANT_LEN,
  WANT_CMD,
  WANT_PAYLOAD,
  WANT_CRC_HI,
  WANT_CRC_LO,
};

struct frame {
  uint8_t len;
  uint8_t cmd;
  uint8_t payload[FRAME_MAX_PAYLOAD];
};

struct parser {
  enum parser_state state;
  uint16_t crc;      // running CRC over LEN, CMD, payload
  uint8_t crc_hi;    // first received CRC byte, parked until the second
  uint8_t got;       // payload bytes received so far
  struct frame work; // assembled in place
};

enum rx_event parser_feed(struct parser *p, uint8_t byte, struct frame *out) {
  switch (p->state) {
  case HUNT_SOF:
    if (byte == FRAME_SOF) {
      p->state = WANT_LEN;
    }
    return RX_NONE;

  case WANT_LEN:
    // Refuse impossible lengths HERE, before they size anything. A LEN of
    // 200 aimed at a 32-byte buffer is 14.02's bounds lesson at the door.
    if (byte > FRAME_MAX_PAYLOAD) {
      p->state = HUNT_SOF;
      return RX_BAD_LEN;
    }
    p->work.len = byte;
    p->crc = crc16_step(0xFFFFu, byte);
    p->state = WANT_CMD;
    return RX_NONE;

  case WANT_CMD:
    p->work.cmd = byte;
    p->crc = crc16_step(p->crc, byte);
    p->got = 0;
    p->state = (p->work.len > 0u) ? WANT_PAYLOAD : WANT_CRC_HI;
    return RX_NONE;

  case WANT_PAYLOAD:
    p->work.payload[p->got] = byte;
    p->crc = crc16_step(p->crc, byte);
    ++p->got;
    if (p->got == p->work.len) {
      p->state = WANT_CRC_HI;
    }
    return RX_NONE;

  case WANT_CRC_HI:
    p->crc_hi = byte;
    p->state = WANT_CRC_LO;
    return RX_NONE;

  case WANT_CRC_LO: {
    // Whatever the verdict, the next byte belongs to a NEW hunt. Going back
    // to WANT_LEN instead is the classic resync bug: the next frame's SOF
    // gets eaten as a length and the stream never recovers.
    p->state = HUNT_SOF;
    const uint16_t received = (uint16_t)(((uint16_t)p->crc_hi << 8) | byte);
    if (received != p->crc) {
      return RX_BAD_CRC;
    }
    *out = p->work;
    return RX_FRAME;
  }
  }
  return RX_NONE; // unreachable; -Wswitch has the enum covered
}

// --- tests -----------------------------------------------------------------------

// Feeds a byte string to a parser and tallies what comes out.
struct tally {
  uint32_t ok, bad_crc, bad_len;
  uint8_t cmds[8];
  uint32_t ncmds;
};

static void run_bytes(struct parser *p, struct tally *t, const uint8_t *bytes,
                      uint32_t n) {
  struct frame f;
  for (uint32_t i = 0; i < n; ++i) {
    switch (parser_feed(p, bytes[i], &f)) {
    case RX_FRAME:
      ++t->ok;
      if (t->ncmds < 8u) {
        t->cmds[t->ncmds++] = f.cmd;
      }
      break;
    case RX_BAD_CRC:
      ++t->bad_crc;
      break;
    case RX_BAD_LEN:
      ++t->bad_len;
      break;
    case RX_NONE:
      break;
    }
  }
}

TEST("the CRC speaks CCITT-FALSE") {
  CHECK_EQ(crc16((const uint8_t *)"123456789", 9), 0x29B1u);
}

TEST("parser: a clean frame, fed a byte at a time") {
  struct parser p = {0};
  struct tally t = {0};
  const uint8_t ping[] = {0xAA, 0x00, 0x01, 0x0D, 0x2E}; // CRC(00 01) = 0x0D2E
  run_bytes(&p, &t, ping, sizeof ping);
  CHECK_EQ(t.ok, 1u);
  CHECK_EQ(t.cmds[0], 0x01u);
}

TEST("parser: an impossible LEN is refused at the door") {
  struct parser p = {0};
  struct frame f;
  CHECK_EQ(parser_feed(&p, 0xAA, &f), RX_NONE);
  CHECK_EQ(parser_feed(&p, 0xC8, &f), RX_BAD_LEN); // LEN 200 into 32 bytes: no
  // ... and the parser is hunting again, not half-inside a ghost frame:
  const uint8_t ping[] = {0xAA, 0x00, 0x01, 0x0D, 0x2E};
  struct tally t = {0};
  run_bytes(&p, &t, ping, sizeof ping);
  CHECK_EQ(t.ok, 1u);
}

TEST("parser: life continues after a bad CRC") {
  struct parser p = {0};
  struct tally t = {0};
  const uint8_t stream[] = {
      0xAA, 0x00, 0x01, 0x0D, 0x2F, // PING, last CRC byte corrupted
      0xAA, 0x00, 0x03, 0x2D, 0x6C, // STATUS, clean -- must still parse
  };
  run_bytes(&p, &t, stream, sizeof stream);
  CHECK_EQ(t.bad_crc, 1u);
  CHECK_EQ(t.ok, 1u);
  CHECK_EQ(t.cmds[0], 0x03u);
}

TEST("the real wire: garbage, corruption, and three good frames") {
  systick_init_1khz();
  uart_rx_init();
  prompt("[mect-feed] ready");

  struct parser p = {0};
  struct tally t = {0};
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < 10000u && (t.ok + t.bad_crc + t.bad_len) < 5u) {
    uint8_t byte;
    while (rx_pop(&byte)) {
      run_bytes(&p, &t, &byte, 1);
    }
  }

  CHECK_EQ(t.ok, 3u);      // PING, STATUS, LED
  CHECK_EQ(t.bad_crc, 1u); // the corrupted PING
  CHECK_EQ(t.bad_len, 1u); // the stray SOF whose "length" was 0xFF
  CHECK_EQ(t.ncmds, 3u);
  CHECK_EQ(t.cmds[0], 0x01u);
  CHECK_EQ(t.cmds[1], 0x03u);
  CHECK_EQ(t.cmds[2], 0x02u);
  CHECK_EQ(rx_overruns, 0u);
}
