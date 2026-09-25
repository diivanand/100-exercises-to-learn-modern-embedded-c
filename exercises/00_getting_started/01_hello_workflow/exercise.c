// =============================================================================
//  00.01 -- The workflow
// =============================================================================
//
//  Welcome. This course is 100 small C programs (plus a bonus track on real
//  hardware), and every one of them works the same way:
//
//   - The header comment -- this text -- is the teaching material. There is
//     no separate book; everything you need is next to the code it is about.
//   - The code below it is yours to edit. The work is marked with TODO.
//   - The tests at the bottom are the specification. Read them; several
//     exercises have a test that exists specifically to catch a plausible
//     shortcut.
//
//  Your loop, from the repository root:
//
//   1. Run `./mec next`. It builds and runs the first exercise that is not
//      yet passing -- this one, right now -- and shows you its failure.
//   2. Open the file it names.
//   3. Make the tests pass.
//   4. Repeat.
//
//  TEST() and CHECK_EQ() come from mect, the course's test harness. It is
//  ~400 lines of plain C in third_party/mect/ and worth reading once you are
//  a few chapters in -- by the end of chapter 07 you will understand every
//  line of it, including how CHECK_EQ knows the type of its arguments.
//
//  THE PROBLEM AT HAND. A 12-bit ADC (analogue-to-digital converter -- the
//  peripheral that turns a voltage into a number) gives you a raw reading
//  between 0 and 4095, where 0 means 0 mV and 4095 means 3300 mV, the supply
//  voltage. The conversion is:
//
//      millivolts = raw * 3300 / 4095
//
//  Order matters: multiply first, then divide. Integer division throws the
//  remainder away, so `raw / 4095 * 3300` is 0 for every raw below 4095.
//  Chapter 02 has much more to say about integer arithmetic; for now the
//  32-bit unsigned type below is comfortably big enough for the product
//  (4095 * 3300 fits with room to spare).
//
//  TASK
//    Implement `adc_to_millivolts`. Do not change the tests.
//
//  RUN IT
//    ./mec test 00_01     (or just ./mec next)
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

uint32_t adc_to_millivolts(uint32_t raw) {
  // TODO: convert a 0..4095 reading into 0..3300 millivolts.
  (void)raw;
  return 0;
}

TEST("full scale reads the supply voltage") {
  CHECK_EQ(adc_to_millivolts(4095), 3300u);
}

TEST("zero reads zero") {
  CHECK_EQ(adc_to_millivolts(0), 0u);
}

TEST("mid-scale and a couple of real readings") {
  CHECK_EQ(adc_to_millivolts(2048), 1650u); // 2048 * 3300 / 4095 = 1650.5..
  CHECK_EQ(adc_to_millivolts(1241), 1000u); // ~1 V
  CHECK_EQ(adc_to_millivolts(100), 80u);    // divide-first gets this one wrong
}
