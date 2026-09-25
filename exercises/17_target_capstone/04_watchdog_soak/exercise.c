// =============================================================================
//  17.04 -- The watchdog soak: ship it
// =============================================================================
//
//  The last exercise. The station runs unattended under hostile traffic
//  with an INDEPENDENT WATCHDOG armed: feed the IWDG at least once a
//  second or the hardware resets the chip. That is the shipping contract
//  for firmware -- not "it never fails", but "when it wedges, it comes
//  back".
//
//  THREE IDEAS MEET HERE:
//
//  1. FEED FROM THE MAIN LOOP, ON HEALTH. The one thing a watchdog can
//     verify is "the loop still goes round". The classic self-defeating
//     move is feeding it from a timer ISR: interrupts keep firing while
//     the main loop is wedged solid, the dog is fed, and the product hangs
//     forever, watchdog and all. The feed lives at the bottom of the
//     superloop and nowhere else.
//
//  2. RECOVERY MUST NOT BLOCK. The starter "recovers" from a corrupted
//     frame by pausing two seconds to let the line settle. Two seconds of
//     not feeding a one-second watchdog: the board resets in the middle of
//     the soak. The parser already resynchronises (17.01); recovery is a
//     counter and a shrug.
//
//  3. RESETS LEAVE FINGERPRINTS. RCC->CSR remembers WHY the chip last
//     reset (RM0351 6.4.29); the first test reads it and fails if the
//     previous run died by watchdog -- which is exactly how you will see
//     the starter's bug: flash, watch the board silently reboot mid-soak,
//     and read the confession on the SECOND boot. A word parked at the top
//     of SRAM2 -- which no linker section touches, and which survives any
//     reset short of power-off -- distinguishes that wedge from the
//     documented, expected reset below. 15.02 met RAM retention as a
//     hazard; shipped firmware uses it exactly like this, for crash
//     breadcrumbs.
//
//  THE ONE THING YOU CANNOT HAVE: a clean exit. An IWDG, once started,
//  cannot be stopped by software -- by design. So after a PASSING run
//  prints its summary, the board watchdog-resets about a second later
//  (the harness has already reported; the run recorded). The suite then
//  re-runs unattended, and with no host feeding it frames, the soak's
//  traffic checks fail on those unattended laps -- reflash or unplug when
//  you are done watching. The dog does not apologise.
//
//  TASK
//    Fix the recovery path in handle_event. Read iwdg_init_1s and its
//    arithmetic on the way down -- the /256 prescaler and reload of 124
//    are doing datasheet-worst-case work.
//
//  RUN IT
//    ./mec flash 17_04        (twice, after fixing: watch forensics pass)
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

#define USART_ISR_ORE (1u << 3)   // RM0351 40.8.8
#define USART_ICR_ORECF (1u << 3) // RM0351 40.8.9

// Reset-cause forensics (RM0351 6.4.29). Not in bsp/l476_regs.h.
#define RCC_CSR_REG (*(volatile uint32_t *)(0x40021000u + 0x94u))
#define RCC_CSR_IWDGRSTF (1u << 29) // "an IWDG reset happened"
#define RCC_CSR_RMVF (1u << 23)     // write 1: clear all reset-cause flags

// A breadcrumb that survives reset: a word at the top of SRAM2, which the
// linker script never touches and startup neither copies nor zeroes. RAM
// retains its contents across any reset short of power loss -- 15.02 met
// that as a hazard; here it is a tool (crash forensics do exactly this).
#define BOOT_MARKER (*(volatile uint32_t *)0x10007FF0u)
#define MARKER_EXPECTED_RESET 0x57A7D06Eu

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

// --- given: the watchdog itself ----------------------------------------------------
//
// The arithmetic: the IWDG counts down on the ~32 kHz LSI (starting the
// watchdog forces LSI on, RM0351 32.3.2). Prescaler /256 gives 128 counts
// per second; a reload of 124 is therefore ~0.98 s from feed to reset --
// with LSI's tolerance (29.5..34 kHz), anywhere from 0.92 s to 1.06 s.
// Size watchdog windows in datasheet-worst-case, never in typicals.
//
// Once started, the IWDG CANNOT be stopped -- not by software, not by a
// debugger detach. That is the point of it, and it has a consequence for
// this test, described at the bottom.

static void iwdg_init_1s(void) {
  IWDG->KR = IWDG_KEY_START;  // counting begins; LSI forced on
  IWDG->KR = IWDG_KEY_UNLOCK; // open PR/RLR for writing
  IWDG->PR = 6;               // /256
  IWDG->RLR = 124;
  while (IWDG->SR != 0u) { // PVU/RVU: the writes cross into the LSI domain
  }
  IWDG->KR = IWDG_KEY_FEED;
}

static void iwdg_feed(void) {
  IWDG->KR = IWDG_KEY_FEED;
}

// --- the soak (yours) ---------------------------------------------------------------

static bool g_prior_watchdog;

// Process everything the wire delivered; return through quickly. The
// watchdog contract belongs to the CALLER's loop -- one feed per healthy
// lap -- so nothing in here may block. Error recovery that stalls the loop
// is how watchdogs bite the innocent: count it, resync (the parser already
// did), move on.
static void handle_event(enum rx_event ev, uint32_t *ok, uint32_t *bad) {
  switch (ev) {
  case RX_FRAME:
    ++*ok;
    break;
  case RX_BAD_CRC:
    ++*bad;
    // TODO: "give the line two seconds to settle." Reread idea 2 in the
    // header, then look at what this pause does to a one-second watchdog.
    {
      const uint32_t settle_start = g_ticks;
      while ((g_ticks - settle_start) < 2000u) {
      }
    }
    break;
  case RX_BAD_LEN:
    ++*bad;
    break;
  case RX_NONE:
    break;
  }
}

// --- tests -----------------------------------------------------------------------

TEST("forensics: the previous run did not die by watchdog") {
  const bool watchdog_reset = (RCC_CSR_REG & RCC_CSR_IWDGRSTF) != 0u;
  const bool expected = (BOOT_MARKER == MARKER_EXPECTED_RESET);

  BOOT_MARKER = 0;             // consume the breadcrumb
  RCC_CSR_REG |= RCC_CSR_RMVF; // clear the cause flags for the next reader

  if (watchdog_reset && !expected) {
    g_prior_watchdog = true;
    FAIL("the LAST run was reset by the IWDG mid-soak: the loop wedged");
  }
  CHECK(1); // either a clean boot, or the documented post-summary reset
}

TEST("the soak: four seconds of hostile traffic, watchdog armed") {
  if (g_prior_watchdog) {
    FAIL("soak skipped: fix the wedge, then reflash");
  }

  systick_init_1khz();
  uart_rx_init();
  iwdg_init_1s(); // from here on, stall for a second and the board reboots

  prompt("[mect-feed] ready");

  struct parser p = {0};
  struct frame f;
  uint32_t ok = 0;
  uint32_t bad = 0;
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < 4000u) {
    uint8_t byte;
    while (rx_pop(&byte)) {
      handle_event(parser_feed(&p, byte, &f), &ok, &bad);
    }
    // Fed from the MAIN LOOP, once per lap: the feed certifies "the loop
    // is still going round", which is the one thing a watchdog can check.
    // Feeding from a timer interrupt certifies only that interrupts still
    // fire -- a wedged main loop with a healthy ISR is precisely the
    // failure a watchdog exists to catch, and that arrangement hides it.
    iwdg_feed();
  }

  CHECK(ok >= 3u);  // the good frames made it through the garbage
  CHECK(bad >= 1u); // and the corrupt one was counted, not fatal
  CHECK_EQ(rx_overruns, 0u);

  // The watchdog cannot be stopped, and the tests are about to end. Leave
  // the breadcrumb that tells the NEXT boot its reset was this run's
  // scheduled death, not a wedge.
  BOOT_MARKER = MARKER_EXPECTED_RESET;
  prompt("(soak done; the board will watchdog-reset shortly after the "
         "summary -- that reboot is expected)");
}
