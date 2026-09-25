// Solution -- 11.03 Multi-bit fields: clear, then set

#include <mect/mect.h>

#include <stdint.h>

// GPIO pin modes, RM0351 8.5.1: two bits per pin in MODER.
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT 1u
#define GPIO_MODE_ALTERNATE 2u
#define GPIO_MODE_ANALOG 3u

void gpio_set_mode(volatile uint32_t *moder, unsigned pin, uint32_t mode) {
  // Read once, edit the copy, write once. Clearing the field first makes
  // the operation correct whatever the field held; doing it on a local
  // copy means the register never holds a half-edited value (a read-modify
  // -write straight on *moder would pass through "input" on the way from
  // analog to output -- one bus cycle of glitch that real peripherals do
  // notice).
  uint32_t value = *moder;
  value &= ~(3u << (pin * 2u));
  value |= (mode & 3u) << (pin * 2u);
  *moder = value;
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
