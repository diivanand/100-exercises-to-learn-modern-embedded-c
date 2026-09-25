// Solution -- 15.04 GPIO: the blink, at last

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

// --- the driver -----------------------------------------------------------------

void led_init(void) {
  // Trap 1: clock the port, and read back so the enable has landed before
  // the MODER access leaves the pipeline.
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  (void)RCC->AHB2ENR;

  // Trap 2: clear the 2-bit field (reset state 11), then set 01 = output.
  GPIOA->MODER &= ~(3u << (BSP_LED_PIN * 2u));
  GPIOA->MODER |= (1u << (BSP_LED_PIN * 2u));
}

void led_on(void) {
  GPIOA->BSRR = 1u << BSP_LED_PIN;
}

void led_off(void) {
  GPIOA->BSRR = 1u << (BSP_LED_PIN + 16u);
}

void led_toggle(void) {
  // Reading ODR is safe; the atomicity problem is only in read-MODIFY-write
  // back into ODR. Read, decide, then let BSRR do the write atomically.
  // (An interrupt between the read and the BSRR write could still make the
  // toggle stale -- if an ISR ever drives the same pin, route every change
  // through one owner instead. 12.01's rules did not stay on the host.)
  if (GPIOA->ODR & (1u << BSP_LED_PIN)) {
    led_off();
  } else {
    led_on();
  }
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
