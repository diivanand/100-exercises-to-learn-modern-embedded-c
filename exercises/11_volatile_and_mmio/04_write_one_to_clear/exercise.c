// =============================================================================
//  11.04 -- Write-1-to-clear: |= acknowledges interrupts you never saw
// =============================================================================
//
//  Not every register is memory with a fancy address. Status and pending
//  registers are often "rc_w1" in ST's notation: read to see the flags,
//  WRITE A 1 TO CLEAR a flag, writes of 0 are ignored. On this part that
//  includes the EXTI pending register (PR1, RM0351 14.5.6), the USART's
//  interrupt flag clear register (ICR, 40.8.9) and the DMA's (IFCR,
//  11.6.2).
//
//  Hardware is built this way for a good reason: it makes acknowledgement
//  RACE-FREE. "Clear bit 13" as a read-modify-write could lose a bit-6
//  event that arrives between the read and the write. With w1c, clearing
//  is a single store naming only the bits you mean, and the peripheral
//  keeps everything you did not name.
//
//  Which makes the ordinary C habit a trap:
//
//      exti->PR1 |= EXTI_LINE_13;     // read PR1, OR in bit 13, write back
//
//  The read picks up EVERY currently pending line as 1s. The write then
//  writes those 1s back -- and on a w1c register, WRITING 1 CLEARS. You
//  have just acknowledged every pending interrupt on the port, including
//  the ones whose handlers have not run. The symptom in the field: under
//  load, the radio "misses" interrupts -- but only when two arrive close
//  together. (MISRA C and every vendor HAL spell the acknowledge as a
//  plain assignment for exactly this reason.)
//
//      exti->PR1 = EXTI_LINE_13;      // clears line 13, touches nothing else
//
//  THE MODEL BELOW gives the register w1c semantics through a read/write
//  pair the tests own (a fake bus, so the semantics are visible on the
//  host). `exti_acknowledge` is the code under test.
//
//  TASK
//    Fix `exti_acknowledge` so it clears exactly `line_mask`. Do not change
//    the fake hardware or the tests.
//
//  RUN IT
//    ./mec test 11_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

#define EXTI_LINE_6 (1u << 6)   // say, a radio's IRQ pin
#define EXTI_LINE_13 (1u << 13) // the blue button on the NUCLEO

// --- the simulated hardware (given; the tests play the peripheral) ------------
// A pending register with rc_w1 semantics: reading returns the pending
// lines; WRITING A 1 CLEARS that line; writing 0 does nothing.

struct fake_exti {
  uint32_t pending;
};

static uint32_t exti_read_pr(const struct fake_exti *exti) {
  return exti->pending;
}

static void exti_write_pr(struct fake_exti *exti, uint32_t value) {
  exti->pending &= ~value; // every 1 written clears; 0s are ignored
}

// --- the code under test -------------------------------------------------------

void exti_acknowledge(struct fake_exti *exti, uint32_t line_mask) {
  // TODO: this is |= spelled through the bus: it reads every pending line
  // and writes them all back as 1s -- which, on w1c, clears them all.
  exti_write_pr(exti, exti_read_pr(exti) | line_mask);
}

TEST("acknowledging one line leaves the other pending") {
  struct fake_exti exti = {.pending = EXTI_LINE_13 | EXTI_LINE_6};

  exti_acknowledge(&exti, EXTI_LINE_13);

  CHECK_EQ(exti_read_pr(&exti) & EXTI_LINE_13, 0u); // ours: acknowledged
  CHECK_EQ(exti_read_pr(&exti) & EXTI_LINE_6, EXTI_LINE_6); // theirs: SURVIVES
}

TEST("acknowledging a line that is not pending changes nothing") {
  struct fake_exti exti = {.pending = EXTI_LINE_6};
  exti_acknowledge(&exti, EXTI_LINE_13);
  CHECK_EQ(exti_read_pr(&exti), EXTI_LINE_6);
}
