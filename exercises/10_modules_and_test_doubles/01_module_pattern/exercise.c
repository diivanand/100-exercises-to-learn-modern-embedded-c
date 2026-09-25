// =============================================================================
//  10.01 -- The module pattern: hardware is a parameter
// =============================================================================
//
//  This chapter is the hinge of the course. Everything before it was about
//  the language; everything after it is about hardware. The idea that
//  connects them, from "Test-Driven Development for Embedded C" (Grenning),
//  is this: FIRMWARE THAT TAKES ITS HARDWARE AS A PARAMETER CAN BE TESTED
//  WITHOUT THE HARDWARE. That single design move is why the first hundred
//  exercises of an embedded course can run on your Mac at all -- Grenning
//  calls it dual-targeting (ch. 5), and it is how serious firmware teams
//  escape the target-hardware bottleneck.
//
//  The vehicle is Grenning's own opening example (ch. 3-4): a driver for 16
//  LEDs behind one memory-mapped 16-bit latch. The module pattern, in C:
//
//   - a SMALL API: init, turn_on, turn_off -- nothing else is anybody's
//     business;
//   - ALL state static inside the module (06.02): callers cannot corrupt
//     what they cannot name;
//   - the hardware INJECTED, not hard-coded: init takes a pointer to the
//     latch. On the board that is (volatile uint16_t *)0x60000000; in the
//     tests below it is an ordinary variable. The driver cannot tell, which
//     is the whole point.
//
//  One hardware truth shapes the design: the latch is WRITE-ONLY. Reading
//  it returns bus garbage, so the driver keeps a SHADOW IMAGE of what it
//  last wrote and composes every update from that (the last test enforces
//  it -- Grenning ch. 4 tells the same story).
//
//  The starter is a plausible first draft with two classic faults:
//
//   1. init() stores the pointer but never DRIVES the latch, so whatever
//      the power-up state was, it stays. "Initialise the hardware" means
//      write to it, not remember it.
//   2. The LED-number-to-bit conversion is off by one: the datasheet counts
//      LEDs from 1, the bus counts bits from 0, and `1u << led` quietly
//      lights the wrong LED -- and misses LED 16 entirely. This is the very
//      bug Grenning walks into on purpose in ch. 3.
//
//  And its bounds check accepts led == 0, which today merely lights a
//  phantom LED -- but the moment you fix the off-by-one, `led - 1u` on
//  led == 0 underflows to 4294967295 (01.02) and the shift becomes undefined
//  behaviour (02.04). Fix the bounds check FIRST, then the conversion; the
//  order in which you repair things is part of the craft.
//
//  TASK
//    Fix led_driver_init, led_bit and the bounds policy. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 10_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- led_driver module --------------------------------------------------------

static volatile uint16_t *led_reg; // the injected latch
static uint16_t led_image;         // shadow of what we last wrote (write-only HW)

enum { LED_FIRST = 1, LED_LAST = 16 };

static bool led_in_range(unsigned led) {
  // TODO: this accepts led == 0, and led_bit(0) shifts by 4294967295.
  return led <= LED_LAST;
}

static uint16_t led_bit(unsigned led) {
  // TODO: LED 1 must be bit 0.
  return (uint16_t)(1u << led);
}

void led_driver_init(volatile uint16_t *reg) {
  // TODO: remembering the latch is not initialising it.
  led_reg = reg;
  led_image = 0;
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
