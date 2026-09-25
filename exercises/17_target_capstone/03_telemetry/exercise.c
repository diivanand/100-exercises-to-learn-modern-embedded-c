// =============================================================================
//  17.03 -- Telemetry: the station speaks unprompted
// =============================================================================
//
//  So far the station only answers. Real devices also REPORT: a heartbeat
//  frame on a schedule, so the other end can tell "quiet" from "dead" and
//  watch the vitals drift. This exercise adds a 1 Hz telemetry frame:
//
//    CMD 0x84, payload: seq (1 byte) | VDDA in mV (2 bytes, BIG-ENDIAN)
//                       | uptime seconds (1 byte, saturating)
//
//  The sequence number is the receiver's loss detector -- one per SENT
//  frame, no gaps, no repeats. The voltage comes from the ADC measuring the
//  internal bandgap reference against the supply (the given adc_init is the
//  full RM0351 bring-up dance: regulator, calibration, ADRDY, long sample
//  time -- read it, 16.04 made you earn each step). The schedule is 13.05's
//  drift-free re-arm, given.
//
//  YOUR PART is telemetry_send: build the frame bytes and hand them to
//  send_raw. The starter builds them WRONG in the two ways wire code goes
//  wrong most:
//
//  1. IT CRCS THE WRONG SPAN. The format says the CRC covers LEN, CMD and
//     payload. The starter covers only the payload. Every heartbeat fails
//     the receiver's check -- and the test plays receiver: it re-runs the
//     CRC over each logged frame and believes nothing else. (Note what the
//     test does NOT do: compare against a hardcoded CRC constant. The
//     values change with the voltage; the CONTRACT doesn't.)
//
//  2. IT PACKS THE VOLTAGE LITTLE-ENDIAN. 02.06 said it: the wire has one
//     byte order, declared by the format, and it is not "whatever my CPU
//     does". 3300 mV is 0x0CE4; swapped, it decodes as 58380 mV -- the test
//     asks whether your board is electrically plausible.
//
//  RUN IT
//    ./mec flash 17_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

#define USART_ISR_ORE (1u << 3)   // RM0351 40.8.8
#define USART_ICR_ORECF (1u << 3) // RM0351 40.8.9

// --- given: tick + prompt -------------------------------------------------------

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

// --- given: RX ring + ISR (17.01) ------------------------------------------------

enum { RX_RING_SIZE = 64 };

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint32_t rx_head, rx_tail, rx_overruns;

void USART2_IRQHandler(void) {
  if ((USART2->ISR & USART_ISR_ORE) != 0u) {
    USART2->ICR = USART_ICR_ORECF;
    ++rx_overruns;
  }
  while ((USART2->ISR & USART_ISR_RXNE) != 0u) {
    const uint8_t byte = (uint8_t)USART2->RDR;
    if ((rx_head - rx_tail) < (uint32_t)RX_RING_SIZE) {
      rx_ring[rx_head % (uint32_t)RX_RING_SIZE] = byte;
      ++rx_head;
    }
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

// --- given: CRC + parser (17.01, correct) ----------------------------------------

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

enum { FRAME_MAX_PAYLOAD = 32, FRAME_SOF = 0xAA };

enum rx_event { RX_NONE, RX_FRAME, RX_BAD_CRC, RX_BAD_LEN };

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
  uint16_t crc;
  uint8_t crc_hi;
  uint8_t got;
  struct frame work;
};

static enum rx_event parser_feed(struct parser *p, uint8_t byte, struct frame *out) {
  switch (p->state) {
  case HUNT_SOF:
    if (byte == FRAME_SOF) {
      p->state = WANT_LEN;
    }
    return RX_NONE;
  case WANT_LEN:
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
    p->state = HUNT_SOF;
    const uint16_t received = (uint16_t)(((uint16_t)p->crc_hi << 8) | byte);
    if (received != p->crc) {
      return RX_BAD_CRC;
    }
    *out = p->work;
    return RX_FRAME;
  }
  }
  return RX_NONE;
}

// --- given: raw-frame transmit with a byte-accurate log --------------------------

struct raw_frame {
  uint8_t n;
  uint8_t bytes[16];
};

enum { LOG_SIZE = 12 };
static struct raw_frame g_log[LOG_SIZE];
static uint32_t g_log_count;

static void send_raw(const uint8_t *bytes, uint8_t n) {
  if (g_log_count < (uint32_t)LOG_SIZE && n <= 16u) {
    g_log[g_log_count].n = n;
    for (uint8_t i = 0; i < n; ++i) {
      g_log[g_log_count].bytes[i] = bytes[i];
    }
    ++g_log_count;
  }
  for (uint8_t i = 0; i < n; ++i) {
    bsp_uart_putc((char)bytes[i]);
  }
}

// Correct framing for REPLIES (the PING handler uses it). Your telemetry
// builder below must produce bytes this good.
static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len) {
  uint8_t buf[16];
  buf[0] = FRAME_SOF;
  buf[1] = len;
  buf[2] = cmd;
  for (uint8_t i = 0; i < len && i < 12u; ++i) {
    buf[3u + i] = payload[i];
  }
  const uint16_t crc = crc16(&buf[1], (uint32_t)len + 2u);
  buf[3u + len] = (uint8_t)(crc >> 8);
  buf[4u + len] = (uint8_t)(crc & 0xFFu);
  send_raw(buf, (uint8_t)(len + 5u));
}

// --- given: VDDA via the internal reference (RM0351 ch. 16) ----------------------
//
// VREFINT is a ~1.212 V bandgap wired to ADC1_IN0. The factory measured it
// at VDDA = 3.0 V and stored the reading (VREFINT_CAL, 5.02 in the
// datasheet's memory map). Reading it again tells you today's VDDA:
//
//     VDDA = 3000 mV * VREFINT_CAL / reading
//
// The bring-up dance is the part people get wrong; each step below is a
// gate the next one needs. Skipping the regulator wait or the calibration
// yields readings that are wrong by just enough to pass a glance.

static void busy_wait_short(void) {
  for (volatile uint32_t i = 0; i < 200u; ++i) { // > 25 us at 4 MHz
  }
}

static void adc_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;

  // Clock the ADC synchronously from HCLK (CKMODE = 01, RM0351 16.4.3);
  // the asynchronous option needs a CCIPR selection we don't require.
  ADC_COMMON->CCR = (ADC_COMMON->CCR & ~(3u << 16)) | (1u << 16);
  ADC_COMMON->CCR |= ADC_CCR_VREFEN; // switch the bandgap onto IN0

  ADC1->CR &= ~ADC_CR_DEEPPWD; // it resets in deep power-down
  ADC1->CR |= ADC_CR_ADVREGEN; // internal regulator up...
  busy_wait_short();           // ...after T_ADCVREG_STUP (20 us)

  ADC1->CR |= ADC_CR_ADCAL; // offset calibration
  while ((ADC1->CR & ADC_CR_ADCAL) != 0u) {
  }

  ADC1->ISR = ADC_ISR_ADRDY; // w1c any stale flag
  ADC1->CR |= ADC_CR_ADEN;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0u) {
  }

  // Channel 0, one conversion, longest sample time -- the bandgap is a
  // high-impedance source and needs its 4 us (SMP0 = 111: 640.5 cycles).
  ADC1->SMPR1 |= 7u;
  ADC1->SQR1 = 0; // L = 0 (one conversion), SQ1 = channel 0
}

static uint16_t vdda_millivolts(void) {
  ADC1->CR |= ADC_CR_ADSTART;
  uint32_t guard = 0;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0u && ++guard < 1000000u) {
  }
  const uint32_t raw = ADC1->DR & 0xFFFFu; // reading DR clears EOC
  if (raw == 0u) {
    return 0;
  }
  return (uint16_t)(3000u * VREFINT_CAL / raw);
}

// --- telemetry (yours) ------------------------------------------------------------

enum { CMD_TELEMETRY = 0x84 };

static uint8_t g_seq;

// Frame 0x84, payload: seq | vdda_mv (big-endian, 2 bytes) | uptime seconds.
static void telemetry_send(void) {
  const uint16_t vdda = vdda_millivolts();
  const uint32_t uptime_s = g_ticks / 1000u;

  uint8_t buf[9];
  buf[0] = FRAME_SOF;
  buf[1] = 4; // LEN
  buf[2] = CMD_TELEMETRY;
  buf[3] = g_seq;
  // TODO: the format says big-endian. This is the CPU's opinion instead.
  buf[4] = (uint8_t)(vdda & 0xFFu);
  buf[5] = (uint8_t)(vdda >> 8);
  buf[6] = uptime_s > 255u ? (uint8_t)255u : (uint8_t)uptime_s;
  // TODO: the CRC's span is LEN..payload, not just the payload.
  const uint16_t crc = crc16(&buf[3], 4);
  buf[7] = (uint8_t)(crc >> 8);
  buf[8] = (uint8_t)(crc & 0xFFu);

  send_raw(buf, 9);
  ++g_seq; // one sent frame, one sequence step: the receiver's loss detector
}

// Given: drift-free 1 Hz schedule (13.05's lesson -- rearm from the DUE
// time, not from "now", or each lap inherits the previous lap's lateness).
static uint32_t g_next_beat;

static void telemetry_tick(void) {
  if ((g_ticks - g_next_beat) < 0x80000000u) { // now >= next, wrap-safe
    telemetry_send();
    g_next_beat += 1000u;
  }
}

// --- tests -----------------------------------------------------------------------

TEST("three heartbeats: monotonic, CRC-true, and electrically plausible") {
  systick_init_1khz();
  uart_rx_init();
  adc_init();

  g_log_count = 0;
  g_seq = 0;
  g_next_beat = g_ticks + 1000u;

  prompt("[mect-feed] ready");

  // Run the station for 3.5 s: commands answered, heartbeats on schedule.
  struct parser p = {0};
  struct frame f;
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < 3500u) {
    uint8_t byte;
    while (rx_pop(&byte)) {
      if (parser_feed(&p, byte, &f) == RX_FRAME && f.cmd == 0x01u) {
        send_frame(0x81, NULL, 0); // PING -> PONG, between heartbeats
      }
    }
    telemetry_tick();
  }

  // Decode the log: every frame must verify against its own CRC.
  uint32_t beats = 0;
  uint32_t pongs = 0;
  uint8_t expected_seq = 0;
  for (uint32_t i = 0; i < g_log_count; ++i) {
    const uint8_t *b = g_log[i].bytes;
    const uint8_t len = b[1];
    CHECK_EQ(b[0], 0xAAu);
    const uint16_t wire_crc = (uint16_t)(((uint16_t)b[3u + len] << 8) | b[4u + len]);
    CHECK_EQ(crc16(&b[1], (uint32_t)len + 2u), wire_crc);

    if (b[2] == 0x84u) {
      ++beats;
      CHECK_EQ(len, 4u);
      CHECK_EQ(b[3], expected_seq); // no gaps, no repeats
      ++expected_seq;
      const uint16_t vdda = (uint16_t)(((uint16_t)b[4] << 8) | b[5]);
      CHECK(vdda >= 3200u); // a USB-powered Nucleo sits near 3300 mV
      CHECK(vdda <= 3400u);
      CHECK(b[6] <= 10u); // uptime, seconds
    } else if (b[2] == 0x81u) {
      ++pongs;
    }
  }
  CHECK_EQ(beats, 3u); // t = 1 s, 2 s, 3 s
  CHECK_EQ(pongs, 2u); // the feed sent two PINGs
  CHECK_EQ(rx_overruns, 0u);
}
