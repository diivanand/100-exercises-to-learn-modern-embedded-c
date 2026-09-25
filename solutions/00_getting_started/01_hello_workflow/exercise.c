// Solution -- 00.01 The workflow

#include <mect/mect.h>

#include <stdint.h>

uint32_t adc_to_millivolts(uint32_t raw) {
  // Multiply first, then divide. The other order truncates to zero for every
  // reading below full scale. The product needs 24 bits at most
  // (4095 * 3300 = 13,513,500), so uint32_t holds it comfortably -- checking
  // that a product fits BEFORE writing the expression is a habit this course
  // will keep asking for (02.03).
  return raw * 3300u / 4095u;
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
