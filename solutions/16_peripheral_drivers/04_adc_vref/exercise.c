// Solution -- 16.04 ADC: measuring your own supply

#include <mect/mect.h>

#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

// Not in bsp/l476_regs.h; defined here against the manual.
#define ADC_CCR_CKMODE_MASK (3u << 16)  // RM0351 16.6.24, CKMODE[1:0]
#define ADC_CCR_CKMODE_HCLK1 (1u << 16) // synchronous, HCLK/1 (AHB presc = 1)

// given: crude cycle burner for the two datasheet wait times (T_ADCVREG_STUP
// = 20 us, and >= 4 ADC clocks after calibration). Generous is free here.
static void burn(uint32_t iterations) {
  for (volatile uint32_t i = 0; i < iterations; ++i) {
  }
}

void adc_init(void) {
  // 1. Clock the ADC block and pick its kernel clock: synchronous HCLK/1
  //    (legal because the AHB prescaler is 1 at reset), plus VREFEN so
  //    channel 0 has something to measure (RM0351 16.4.4, 16.4.32).
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;
  ADC_COMMON->CCR =
      (ADC_COMMON->CCR & ~ADC_CCR_CKMODE_MASK) | ADC_CCR_CKMODE_HCLK1 | ADC_CCR_VREFEN;

  // 2. Out of deep power-down; regulator on; wait its 20 us (RM0351 16.4.6).
  ADC1->CR &= ~ADC_CR_DEEPPWD;
  ADC1->CR |= ADC_CR_ADVREGEN;
  burn(200); // ~20 us at 4 MHz with margin

  // 3. CALIBRATE, while still disabled. The ADC measures its own offset
  //    error and corrects every later sample. Skip this and readings carry
  //    a few counts of DC you will chase for a week (RM0351 16.4.8).
  ADC1->CR |= ADC_CR_ADCAL;
  uint32_t guard = 100000;
  while ((ADC1->CR & ADC_CR_ADCAL) != 0u && --guard != 0u) {
  }
  burn(16); // >= 4 ADC clock cycles between ADCAL clear and ADEN

  // 4. Enable, and wait until it says ready. ADRDY is write-1-to-clear;
  //    clearing a stale flag first is 11.04 hygiene.
  ADC1->ISR = ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  guard = 100000;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0u && --guard != 0u) {
  }

  // 5. Sample time for channel 0: the longest, 640.5 cycles. VREFINT's
  //    source impedance needs >= 4 us of sampling; at HCLK/1 = 4 MHz this
  //    gives 160 us -- generous beats flaky (RM0351 16.4.12).
  ADC1->SMPR1 |= 7u; // SMP0[2:0] = 111
}

uint32_t adc_read_vref_raw(void) {
  // One conversion of channel 0: L[3:0] = 0 means a sequence of one, and
  // SQ1[10:6] = 0 selects channel 0 = VREFINT (RM0351 16.6.11).
  ADC1->SQR1 = 0;
  ADC1->CR |= ADC_CR_ADSTART;
  uint32_t guard = 1000000;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0u && --guard != 0u) {
  }
  return ADC1->DR; // reading DR also clears EOC
}

uint32_t vdda_mv_from_raw(uint32_t raw) {
  // The factory measured VREFINT against VDDA = 3000 mV and stored that
  // reading (VREFINT_CAL). The reference VOLTAGE has not changed on your
  // desk -- your VDDA has. A smaller raw reading therefore means a LARGER
  // supply: raw = CAL * 3000 / VDDA, so VDDA = 3000 * CAL / raw. Keeping
  // the fraction the right way up is the whole function (DS10198 3.15.1).
  return 3000u * (uint32_t)VREFINT_CAL / raw;
}

TEST("the ADC wakes, calibrates and reports ready") {
  adc_init();
  CHECK_EQ(ADC1->CR & ADC_CR_DEEPPWD, 0u);
  CHECK_EQ(ADC1->CR & ADC_CR_ADVREGEN, ADC_CR_ADVREGEN);
  CHECK_EQ(ADC1->CR & ADC_CR_ADCAL, 0u); // calibration finished
  CHECK_EQ(ADC1->ISR & ADC_ISR_ADRDY, ADC_ISR_ADRDY);
}

TEST("a conversion lands in the plausible band") {
  const uint32_t raw = adc_read_vref_raw();
  // CAL was taken at 3.0 V; on a 3.3 V board the reading shrinks by 3.0/3.3.
  CHECK(raw >= 1200u);
  CHECK(raw <= 1800u);
}

TEST("VDDA on a USB-powered Nucleo is about 3.3 V") {
  const uint32_t vdda = vdda_mv_from_raw(adc_read_vref_raw());
  CHECK(vdda >= 3150u);
  CHECK(vdda <= 3450u);
}

TEST("two consecutive measurements agree within 50 mV") {
  const uint32_t a = vdda_mv_from_raw(adc_read_vref_raw());
  const uint32_t b = vdda_mv_from_raw(adc_read_vref_raw());
  const uint32_t diff = (a > b) ? (a - b) : (b - a);
  CHECK(diff <= 50u);
}
