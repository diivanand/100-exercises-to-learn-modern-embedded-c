// Solution -- 17.02 Command handlers: the station answers

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

// --- given: LED driver (15.04) ---------------------------------------------------

static void led_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  (void)RCC->AHB2ENR;
  GPIOA->MODER &= ~(3u << (BSP_LED_PIN * 2u));
  GPIOA->MODER |= (1u << (BSP_LED_PIN * 2u));
}

static void led_on(void) {
  GPIOA->BSRR = 1u << BSP_LED_PIN;
}

static void led_off(void) {
  GPIOA->BSRR = 1u << (BSP_LED_PIN + 16u);
}

static uint8_t led_get(void) {
  return (uint8_t)((GPIOA->ODR >> BSP_LED_PIN) & 1u);
}

static void led_toggle(void) {
  if (led_get() != 0u) {
    led_off();
  } else {
    led_on();
  }
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

// --- given: reply transmission, with a log the tests can read --------------------

struct sent_frame {
  uint8_t cmd;
  uint8_t len;
  uint8_t payload[8];
};

enum { LOG_SIZE = 8 };
static struct sent_frame g_log[LOG_SIZE];
static uint32_t g_log_count;

// Frames the reply, CRCs it, sends it out the wire AND records it, so the
// tests can assert on bytes without a second serial port.
static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len) {
  if (g_log_count < (uint32_t)LOG_SIZE) {
    g_log[g_log_count].cmd = cmd;
    g_log[g_log_count].len = len;
    for (uint8_t i = 0; i < len && i < 8u; ++i) {
      g_log[g_log_count].payload[i] = payload[i];
    }
    ++g_log_count;
  }

  uint16_t crc = crc16_step(0xFFFFu, len);
  crc = crc16_step(crc, cmd);
  bsp_uart_putc((char)FRAME_SOF);
  bsp_uart_putc((char)len);
  bsp_uart_putc((char)cmd);
  for (uint8_t i = 0; i < len; ++i) {
    crc = crc16_step(crc, payload[i]);
    bsp_uart_putc((char)payload[i]);
  }
  bsp_uart_putc((char)(crc >> 8));
  bsp_uart_putc((char)(crc & 0xFFu));
}

// Stats the drain loop maintains and STATUS reports.
static struct {
  uint8_t frames_ok, frames_bad_crc, frames_dropped;
} g_stats;

// --- the dispatch (yours) ---------------------------------------------------------

enum {
  CMD_PING = 0x01,
  CMD_LED = 0x02,
  CMD_STATUS = 0x03,
  REPLY_BASE = 0x80,
  CMD_NAK = 0xFF,
};

static void handle_ping(const struct frame *f) {
  (void)f;
  send_frame(0x81, NULL, 0);
}

static void handle_led(const struct frame *f) {
  // Payload byte: 0 off, 1 on, 2 toggle. Reply carries the RESULTING state
  // -- the caller asked for a change, the answer reports the truth.
  switch (f->len >= 1u ? f->payload[0] : 0xFFu) {
  case 0:
    led_off();
    break;
  case 1:
    led_on();
    break;
  case 2:
    led_toggle();
    break;
  default:
    break; // an unknown argument changes nothing; the state reply says so
  }
  const uint8_t state = led_get();
  send_frame(0x82, &state, 1);
}

static void handle_status(const struct frame *f) {
  (void)f;
  const uint32_t uptime_s = g_ticks / 1000u;
  const uint8_t payload[4] = {
      g_stats.frames_ok,
      g_stats.frames_bad_crc,
      g_stats.frames_dropped,
      uptime_s > 255u ? (uint8_t)255u : (uint8_t)uptime_s, // saturate (02.03)
  };
  send_frame(0x83, payload, 4);
}

static void handle_unknown(const struct frame *f) {
  send_frame(CMD_NAK, &f->cmd, 1);
}

typedef void (*cmd_handler)(const struct frame *f);

// Index = command byte. Valid commands are dense and tiny, so a direct
// table wins over a search -- PROVIDED nothing indexes it with a byte the
// table never heard of (14.04's bounds lesson).
static cmd_handler const handlers[4] = {
    handle_unknown, // 0x00 is not a command
    handle_ping,
    handle_led,
    handle_status,
};

void dispatch(const struct frame *f) {
  // The guard IS the exercise: every byte value must land somewhere ON
  // PURPOSE. Out-of-range commands get the NAK handler, not whatever
  // function pointer happens to live past the table's end.
  if (f->cmd < (uint8_t)(sizeof handlers / sizeof handlers[0])) {
    handlers[f->cmd](f);
  } else {
    handle_unknown(f);
  }
}

// --- tests -----------------------------------------------------------------------

static void reset_station(void) {
  g_log_count = 0;
  g_stats.frames_ok = 0;
  g_stats.frames_bad_crc = 0;
  g_stats.frames_dropped = 0;
}

TEST("unit: an unknown command is NAKed, not dispatched into the void") {
  reset_station();
  const struct frame f = {.len = 0, .cmd = 0x7F};
  dispatch(&f);
  REQUIRE(g_log_count == 1u);
  CHECK_EQ(g_log[0].cmd, 0xFFu);
  CHECK_EQ(g_log[0].payload[0], 0x7Fu);
}

TEST("unit: LED on, toggle, toggle -- replies report real states") {
  reset_station();
  led_init();
  led_off();
  const struct frame on = {.len = 1, .cmd = CMD_LED, .payload = {1}};
  const struct frame tog = {.len = 1, .cmd = CMD_LED, .payload = {2}};
  dispatch(&on);
  dispatch(&tog);
  dispatch(&tog);
  REQUIRE(g_log_count == 3u);
  CHECK_EQ(g_log[0].payload[0], 1u); // on
  CHECK_EQ(g_log[1].payload[0], 0u); // toggled off
  CHECK_EQ(g_log[2].payload[0], 1u); // toggled on again
  led_off();
}

TEST("the wire: a five-command conversation") {
  reset_station();
  led_init();
  systick_init_1khz();
  uart_rx_init();
  prompt("[mect-feed] ready");

  struct parser p = {0};
  struct frame f;
  uint32_t handled = 0;
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < 10000u && handled < 5u) {
    uint8_t byte;
    while (rx_pop(&byte)) {
      switch (parser_feed(&p, byte, &f)) {
      case RX_FRAME:
        ++g_stats.frames_ok;
        dispatch(&f);
        ++handled;
        break;
      case RX_BAD_CRC:
        ++g_stats.frames_bad_crc;
        break;
      case RX_BAD_LEN:
        ++g_stats.frames_dropped;
        break;
      case RX_NONE:
        break;
      }
    }
  }

  // Feed: PING, LED on, LED toggle, unknown 0x7F, STATUS.
  REQUIRE(g_log_count == 5u);
  CHECK_EQ(g_log[0].cmd, 0x81u);
  CHECK_EQ(g_log[1].cmd, 0x82u);
  CHECK_EQ(g_log[1].payload[0], 1u); // on
  CHECK_EQ(g_log[2].cmd, 0x82u);
  CHECK_EQ(g_log[2].payload[0], 0u); // toggled back off
  CHECK_EQ(g_log[3].cmd, 0xFFu);
  CHECK_EQ(g_log[3].payload[0], 0x7Fu);
  CHECK_EQ(g_log[4].cmd, 0x83u);
  CHECK_EQ(g_log[4].payload[0], 5u); // frames_ok includes the STATUS itself
  CHECK_EQ(g_log[4].payload[1], 0u);
  CHECK_EQ(g_log[4].payload[2], 0u);
  CHECK(g_log[4].payload[3] <= 30u); // uptime, seconds
  CHECK_EQ(led_get(), 0u);
}
