// =============================================================================
//  16.04 -- ADC: measuring your own supply
// =============================================================================
//
//  Every analogue reading on this die is a ratio against VDDA, the analogue
//  supply. So before trusting a single sensor sample, measure the ruler:
//  the ADC has an internal bandgap reference (VREFINT, ~1.21 V) on channel
//  0, and ST measured it for YOUR individual chip at the factory, at
//  VDDA = 3000 mV, and burned the reading into system memory --
//  VREFINT_CAL, already mapped in bsp/l476_regs.h (DS10198 3.15.1).
//
//  The arithmetic that follows trips half the people who meet it. The
//  bandgap VOLTAGE is fixed; your VDDA is not 3000 mV. A HIGHER supply
//  makes the fixed reference a SMALLER fraction of full scale, so the raw
//  reading shrinks:  raw = CAL * 3000 / VDDA. Solve for the thing you
//  want:              VDDA = 3000 * CAL / raw.
//  The starter has the fraction upside down -- on a 3.3 V board it reports
//  about 2.7 V, and the tests know what a USB-powered Nucleo reads.
//
//  Before any of that, the L4 ADC must be WOKEN, in order (RM0351 16.4.6,
//  16.4.8, 16.4.9):
//
//    1. clock the block; pick a kernel clock (we use synchronous HCLK/1)
//       and set VREFEN so channel 0 is connected;
//    2. leave deep power-down (DEEPPWD=0), enable the internal regulator
//       (ADVREGEN=1), wait T_ADCVREG_STUP = 20 us;
//    3. CALIBRATE while still disabled: set ADCAL, wait for it to clear.
//       The ADC measures its own offset and subtracts it from every later
//       sample. The starter SKIPS this step -- no test can convict a few
//       counts of offset from software alone, but your readings carry it
//       until you put the step back. The step costs 116 ADC cycles, once;
//    4. enable: clear stale ADRDY (write-1-to-clear, 11.04), set ADEN,
//       wait for ADRDY;
//    5. give channel 0 the longest sample time -- VREFINT's impedance
//       wants >= 4 us of sampling (SMPR1, SMP0 = 111).
//
//  TASK
//    Add the missing calibration step to adc_init, and turn the fraction
//    in vdda_mv_from_raw the right way up.
//
//  RUN IT
//    ./mec flash 16_04
//
// =============================================================================

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
  RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
  (void)RCC->AHB2ENR;
  ADC_COMMON->CCR =
      (ADC_COMMON->CCR & ~ADC_CCR_CKMODE_MASK) | ADC_CCR_CKMODE_HCLK1 | ADC_CCR_VREFEN;

  ADC1->CR &= ~ADC_CR_DEEPPWD;
  ADC1->CR |= ADC_CR_ADVREGEN;
  burn(200); // ~20 us at 4 MHz with margin

  // TODO: step 3 of the wake-up dance is missing here (RM0351 16.4.8).

  ADC1->ISR = ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  uint32_t guard = 100000;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0u && --guard != 0u) {
  }

  ADC1->SMPR1 |= 7u; // SMP0[2:0] = 111: 640.5 cycles, 160 us -- generous
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
  // TODO: this says "a smaller reading means a smaller supply". The header
  // explains why it is exactly the other way round.
  return 3000u * raw / (uint32_t)VREFINT_CAL;
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
