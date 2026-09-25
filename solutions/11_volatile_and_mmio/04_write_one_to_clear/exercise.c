// Solution -- 11.04 Write-1-to-clear: |= acknowledges interrupts you never saw

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
  // Write EXACTLY the bit being acknowledged. The |= habit from ordinary
  // registers reads the whole pending set and writes it all back as 1s --
  // on a w1c register that acknowledges every event, including the ones
  // nobody has serviced yet. The correct acknowledge never reads at all.
  exti_write_pr(exti, line_mask);
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
