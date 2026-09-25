// =============================================================================
//  15.04 -- GPIO: the blink, at last
// =============================================================================
//
//  Every register lesson from chapter 11 lands on real silicon in the next
//  forty lines. The goal: LD2, the green LED on PA5, blinking. The test
//  suite checks the registers; your eyes check the physics.
//
//  Three steps, and a trap in each:
//
//  1. CLOCK THE PORT. GPIOA sits dead until RCC->AHB2ENR bit 0 says
//     otherwise (11's refrain: nothing moves until RCC says so). Read the
//     register back once after setting it -- the write takes a moment to
//     reach the peripheral, and the very next access can race it (RM0351
//     6.2.17). A port with no clock reads as zeros and eats writes, which
//     is why "I configured MODER and nothing happened" is bug number one
//     in every bring-up.
//
//  2. MODE THE PIN. MODER gives each pin two bits: 00 input, 01 output,
//     10 alternate, 11 analog. Here is the trap 11.03 warned about, live:
//     GPIOA's MODER RESETS TO 0xABFFFFFF (RM0351 8.5.1) -- PA5's field
//     starts at 11, analog. `|=` can only turn bits ON; OR-ing 01 into 11
//     leaves 11. You must CLEAR THE FIELD, THEN SET IT.
//
//  3. DRIVE THE PIN. You could read-modify-write ODR. Do not get into the
//     habit: between your read and your write, an interrupt can flip
//     another pin on the same port and your write puts it back (12.01's
//     lost update, in hardware). BSRR exists for this: writing bit n sets
//     pin n, writing bit n+16 clears it, atomically, no read involved.
//     Writing 0 to any BSRR bit does nothing, so there is nothing to
//     preserve.
//
//  The systick pieces from 15.03 are included below as given code.
//
//  TASK
//    Fix led_init (both traps) and implement led_on / led_off / led_toggle
//    with BSRR (toggle may read ODR -- reading is safe; see the solution
//    for the one caveat). Then watch your board.
//
//  RUN IT
//    ./mec flash 15_04        (six blinks after the tests pass)
//
// =============================================================================

#include <mect/mect.h>

#include "bsp.h"
#include "l476_regs.h"

#include <stdint.h>

// --- given: the 15.03 tick ----------------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR =
      SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

static void delay_ms(uint32_t ms) {
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < ms) {
  }
}

// --- yours ----------------------------------------------------------------------

void led_init(void) {
  // TODO: two traps from the header live here. (1) no clock enable;
  // (2) |= into a field that resets to 11.
  GPIOA->MODER |= (1u << (BSP_LED_PIN * 2u));
}

void led_on(void) {
  // TODO: BSRR, set half.
}

void led_off(void) {
  // TODO: BSRR, reset half (bit + 16).
}

void led_toggle(void) {
  // TODO: read ODR, drive the opposite through BSRR.
}

TEST("the port is clocked and the pin is a push-pull output") {
  led_init();
  CHECK_EQ(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN, RCC_AHB2ENR_GPIOAEN);
  // PA5's two MODER bits must read 01 -- not the reset 11.
  CHECK_EQ((GPIOA->MODER >> (BSP_LED_PIN * 2u)) & 3u, 1u);
  CHECK_EQ((GPIOA->OTYPER >> BSP_LED_PIN) & 1u, 0u); // push-pull (reset state)
}

TEST("BSRR drives ODR without touching neighbours") {
  led_on();
  CHECK_EQ((GPIOA->ODR >> BSP_LED_PIN) & 1u, 1u);
  led_off();
  CHECK_EQ((GPIOA->ODR >> BSP_LED_PIN) & 1u, 0u);
  led_toggle();
  CHECK_EQ((GPIOA->ODR >> BSP_LED_PIN) & 1u, 1u);
  led_toggle();
  CHECK_EQ((GPIOA->ODR >> BSP_LED_PIN) & 1u, 0u);
}

TEST("blink: now look at the board") {
  systick_init_1khz();
  for (int i = 0; i < 6; ++i) {
    led_toggle();
    delay_ms(150);
  }
  led_off();
  // No register can vouch for a photon. The check below only records that
  // the loop ran; the assertion that matters is the one you just watched.
  CHECK(1);
}
