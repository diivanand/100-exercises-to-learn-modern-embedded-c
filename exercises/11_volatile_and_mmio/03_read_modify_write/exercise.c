// =============================================================================
//  11.03 -- Multi-bit fields: clear, then set
// =============================================================================
//
//  Half the registers on this die are arrays of small fields. GPIO MODER
//  (RM0351 8.5.1) packs sixteen 2-bit fields into one word -- two bits per
//  pin: 00 input, 01 output, 10 alternate function, 11 analog.
//
//  The habit that works for single FLAG bits,
//
//      reg |= FLAG;      // fine: a 1-bit field can only need setting
//
//  is a bug for anything wider. OR can only raise bits. If pin 5's field
//  currently reads 11 (analog -- which is most pins' RESET state on this
//  part: GPIOA resets to 0xABFFFFFF, RM0351 8.4.1) and you want 01
//  (output), then
//
//      moder |= 01 << 10;      // 11 | 01 = 11: still analog
//
//  changes nothing, and the LED you are trying to blink stays dark. This is
//  the first bug everyone writes against real silicon -- 15.04 meets it on
//  the actual GPIOA -- and it is invisible on any fake register that starts
//  at zero, which is why THIS test initialises its fake to the documented
//  reset value. Test your register code against reset values, not zeros.
//
//  The correct shape reads once, clears the field, sets the new value, and
//  writes once:
//
//      uint32_t v = *moder;
//      v &= ~(3u << (pin * 2));       // clear both bits
//      v |= mode << (pin * 2);        // set the new value
//      *moder = v;
//
//  One write, not two: editing `*moder` in place (`*moder &= ...; *moder
//  |= ...`) would march the pin through an unintended mode between the two
//  stores -- volatile guarantees both stores HAPPEN, which is exactly the
//  problem. A glitch one bus-cycle long is real to hardware.
//
//  TASK
//    Fix `gpio_set_mode`. Do not change the tests.
//
//  RUN IT
//    ./mec test 11_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// GPIO pin modes, RM0351 8.5.1: two bits per pin in MODER.
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT 1u
#define GPIO_MODE_ALTERNATE 2u
#define GPIO_MODE_ANALOG 3u

void gpio_set_mode(volatile uint32_t *moder, unsigned pin, uint32_t mode) {
  // TODO: OR can only raise bits; a field that holds 11 stays 11.
  *moder |= (mode & 3u) << (pin * 2u);
}

TEST("a pin that reset to analog becomes an output") {
  // GPIOA's documented reset value (RM0351 8.4.1): mostly analog (11),
  // with the SWD pins on alternate function. |= alone cannot clear bits.
  volatile uint32_t moder = 0xABFFFFFFu;

  gpio_set_mode(&moder, 5, GPIO_MODE_OUTPUT);

  CHECK_EQ((moder >> 10) & 3u, GPIO_MODE_OUTPUT);
  // And nothing else moved: only bits 11:10 changed.
  CHECK_EQ(moder, (0xABFFFFFFu & ~(3u << 10)) | (1u << 10));
}

TEST("setting the mode a pin already has is a no-op") {
  volatile uint32_t moder = 0xABFFFFFFu;
  gpio_set_mode(&moder, 5, GPIO_MODE_ANALOG);
  CHECK_EQ(moder, 0xABFFFFFFu);
}

TEST("neighbouring pins are untouched") {
  volatile uint32_t moder = 0u; // every pin an input
  gpio_set_mode(&moder, 7, GPIO_MODE_ALTERNATE);
  CHECK_EQ((moder >> 14) & 3u, GPIO_MODE_ALTERNATE);
  CHECK_EQ((moder >> 12) & 3u, GPIO_MODE_INPUT); // pin 6
  CHECK_EQ((moder >> 16) & 3u, GPIO_MODE_INPUT); // pin 8
}
