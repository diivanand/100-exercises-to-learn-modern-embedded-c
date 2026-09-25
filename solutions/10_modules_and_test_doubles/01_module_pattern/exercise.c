// Solution -- 10.01 The module pattern: hardware is a parameter

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- led_driver module --------------------------------------------------------

static volatile uint16_t *led_reg; // the injected latch
static uint16_t led_image;         // shadow of what we last wrote (write-only HW)

enum { LED_FIRST = 1, LED_LAST = 16 };

static bool led_in_range(unsigned led) {
  return led >= LED_FIRST && led <= LED_LAST;
}

static uint16_t led_bit(unsigned led) {
  // LED 1 is bit 0: the datasheet numbers LEDs from one, the bus numbers
  // bits from zero, and this one line is where the two worlds meet. Keeping
  // the conversion in a named helper means it can only be wrong ONCE.
  return (uint16_t)(1u << (led - 1u));
}

void led_driver_init(volatile uint16_t *reg) {
  led_reg = reg;
  led_image = 0;
  *led_reg = led_image; // establish the documented all-off state
}

void led_driver_turn_on(unsigned led) {
  if (!led_in_range(led)) {
    return; // policy: ignore, don't fault -- and never shift by garbage
  }
  led_image = (uint16_t)(led_image | led_bit(led));
  *led_reg = led_image;
}

void led_driver_turn_off(unsigned led) {
  if (!led_in_range(led)) {
    return;
  }
  led_image = (uint16_t)(led_image & (uint16_t)~led_bit(led));
  *led_reg = led_image;
}

// --- tests ----------------------------------------------------------------------

static volatile uint16_t fake_led_register;

TEST("init drives every led off, whatever the latch held") {
  fake_led_register = 0xFFFF;
  led_driver_init(&fake_led_register);
  CHECK_EQ(fake_led_register, 0u);
}

TEST("led 1 is bit 0, led 16 is bit 15") {
  led_driver_init(&fake_led_register);
  led_driver_turn_on(1);
  CHECK_EQ(fake_led_register, 0x0001u);
  led_driver_turn_on(16);
  CHECK_EQ(fake_led_register, 0x8001u);
}

TEST("turning one led off leaves the others alone") {
  led_driver_init(&fake_led_register);
  led_driver_turn_on(3);
  led_driver_turn_on(4);
  led_driver_turn_off(3);
  CHECK_EQ(fake_led_register, 0x0008u); // led 4 = bit 3
}

TEST("out-of-range leds are ignored, not shifted into neighbours") {
  led_driver_init(&fake_led_register);
  led_driver_turn_on(0);
  led_driver_turn_on(17);
  led_driver_turn_off(0);
  CHECK_EQ(fake_led_register, 0u);
}

TEST("the driver trusts its image, never a readback") {
  led_driver_init(&fake_led_register);
  led_driver_turn_on(5);
  fake_led_register = 0xFFFF; // a write-only latch reads back bus garbage
  led_driver_turn_on(6);
  CHECK_EQ(fake_led_register, 0x0030u); // bits 4 and 5, nothing else
}
