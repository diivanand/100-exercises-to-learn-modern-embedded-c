// =============================================================================
//  11.02 -- A peripheral is a struct at an address
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- the _Static_asserts
//  below reject the broken register structs. The asserts are the exercise.
//
//  Open the reference manual to a peripheral chapter and you find a table:
//  register name, offset, reset value. The C spelling of that table is a
//  struct of `volatile uint32_t`, overlaid at the peripheral's base address:
//
//      struct gpio_regs { volatile uint32_t MODER; ... };
//      #define GPIOA ((struct gpio_regs *)0x48000000u)
//      GPIOA->ODR = ...;   // a store to 0x48000014
//
//  Three rules make it work:
//
//   1. DECLARATION ORDER IS THE MEMORY MAP. Each uint32_t advances the
//      offset by 4. There is no padding to fear here -- every member has
//      the same size and alignment -- which is precisely why the idiom is
//      safe in a domain where struct layout is otherwise a portability
//      trap (05.01).
//
//   2. HOLES MUST BE SPELLED OUT. Peripheral maps have gaps (the RCC's map
//      below has three). A gap you do not declare shifts every register
//      after it -- the struct still compiles, the code still runs, and
//      every write lands one register over. Declare `uint32_t reservedN;`
//      for every hole, un-volatile and never touched.
//
//   3. ASSERT EVERY OFFSET THAT MATTERS. `_Static_assert(offsetof(...))`
//      (07.05) turns a miscounted gap from a Saturday of probing registers
//      with a debugger into a compile error that quotes the manual back at
//      you. The course's own bsp/l476_regs.h -- which chapters 15-17 run
//      against real silicon -- is written exactly this way; this exercise
//      is its blueprint.
//
//  Two structs below are wrong. `gpio_regs` collapsed AFRL/AFRH (RM0351
//  8.5.9-8.5.10, offsets 0x20 and 0x24) into one register, so BRR sits at
//  0x24 instead of 0x28. `rcc_regs_excerpt` transcribed the manual's
//  register LIST while skipping its HOLES (nothing lives at 0x24, 0x34,
//  0x44 -- see RM0351 6.4), so everything from AHB1RSTR on is shifted. On
//  the host the tests catch this against a fake array; on the board, the
//  GPIO one means "configuring pin 8's alternate function" silently resets
//  pins instead (BRR!), and the RCC one means clocks never start.
//
//  TASK
//    Fix both structs so the asserts -- which quote the manual -- pass, and
//    the overlay tests agree. Do not change the asserts or the tests.
//
//  RUN IT
//    ./mec test 11_02
//
// =============================================================================

#include <mect/mect.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// --- the register block, per RM0351 ------------------------------------------

struct gpio_regs {
  volatile uint32_t MODER;   // 0x00  RM0351 8.5.1
  volatile uint32_t OTYPER;  // 0x04  RM0351 8.5.2
  volatile uint32_t OSPEEDR; // 0x08  RM0351 8.5.3
  volatile uint32_t PUPDR;   // 0x0C  RM0351 8.5.4
  volatile uint32_t IDR;     // 0x10  RM0351 8.5.5
  volatile uint32_t ODR;     // 0x14  RM0351 8.5.6
  volatile uint32_t BSRR;    // 0x18  RM0351 8.5.7
  volatile uint32_t LCKR;    // 0x1C  RM0351 8.5.8
  // TODO: the manual documents TWO alternate-function registers here.
  volatile uint32_t AFR; // 0x20
  volatile uint32_t BRR; // 0x24 -- but RM0351 8.5.11 says 0x28
};

// An excerpt of the RCC map, which -- unlike GPIO -- has holes in it.
// TODO: this transcribes the manual's register list, but not its gaps.
struct rcc_regs_excerpt {
  volatile uint32_t CR;          // 0x00  RM0351 6.4.1
  volatile uint32_t ICSCR;       // 0x04  RM0351 6.4.2
  volatile uint32_t CFGR;        // 0x08  RM0351 6.4.3
  volatile uint32_t PLLCFGR;     // 0x0C  RM0351 6.4.4
  volatile uint32_t PLLSAI1CFGR; // 0x10  RM0351 6.4.5
  volatile uint32_t PLLSAI2CFGR; // 0x14  RM0351 6.4.6
  volatile uint32_t CIER;        // 0x18  RM0351 6.4.7
  volatile uint32_t CIFR;        // 0x1C  RM0351 6.4.8
  volatile uint32_t CICR;        // 0x20  RM0351 6.4.9
  volatile uint32_t AHB1RSTR;    // RM0351 6.4.10 says 0x28...
  volatile uint32_t AHB2RSTR;    // RM0351 6.4.11 says 0x2C
  volatile uint32_t AHB3RSTR;    // RM0351 6.4.12 says 0x30
  volatile uint32_t APB1RSTR1;   // RM0351 6.4.13 says 0x38
  volatile uint32_t APB1RSTR2;   // RM0351 6.4.14 says 0x3C
  volatile uint32_t APB2RSTR;    // RM0351 6.4.15 says 0x40
  volatile uint32_t AHB1ENR;     // RM0351 6.4.16 says 0x48
  volatile uint32_t AHB2ENR;     // RM0351 6.4.17 says 0x4C
};

// --- the layout contract: these asserts ARE the tests. Do not change them. ----

_Static_assert(offsetof(struct gpio_regs, IDR) == 0x10, "RM0351 8.5.5");
_Static_assert(offsetof(struct gpio_regs, BSRR) == 0x18, "RM0351 8.5.7");
_Static_assert(offsetof(struct gpio_regs, AFR) == 0x20, "RM0351 8.5.9");
_Static_assert(offsetof(struct gpio_regs, BRR) == 0x28, "RM0351 8.5.11");
_Static_assert(sizeof(struct gpio_regs) == 0x2C, "MODER..BRR spans 0x2C bytes");

_Static_assert(offsetof(struct rcc_regs_excerpt, CICR) == 0x20, "RM0351 6.4.9");
_Static_assert(offsetof(struct rcc_regs_excerpt, AHB1RSTR) == 0x28, "RM0351 6.4.10");
_Static_assert(offsetof(struct rcc_regs_excerpt, APB1RSTR1) == 0x38, "RM0351 6.4.13");
_Static_assert(offsetof(struct rcc_regs_excerpt, AHB2ENR) == 0x4C, "RM0351 6.4.17");

// --- runtime checks against a fake block --------------------------------------
// On the board the base address comes from the memory map:
//     #define GPIOA ((struct gpio_regs *)0x48000000u)
// On the host that address is nobody's memory, so the tests overlay the
// struct on an ordinary array and check that member accesses land on the
// words the offsets promise.

TEST("GPIO members land on their documented offsets") {
  uint32_t fake[11] = {0};
  struct gpio_regs *gpio = (struct gpio_regs *)fake;

  gpio->MODER = 0xA8000000u; // GPIOA's actual reset value, RM0351 8.4.1
  gpio->BSRR = (1u << 5);
  gpio->AFR[1] = 0x00000700u;

  CHECK_EQ(fake[0x00 / 4], 0xA8000000u);
  CHECK_EQ(fake[0x18 / 4], 1u << 5);
  CHECK_EQ(fake[0x24 / 4], 0x00000700u); // AFRH = AFR[1] at 0x24
}

TEST("RCC members skip the reserved holes") {
  uint32_t fake[20] = {0};
  struct rcc_regs_excerpt *rcc = (struct rcc_regs_excerpt *)fake;

  rcc->AHB2ENR = 1u; // GPIOAEN
  rcc->APB1RSTR1 = 2u;

  CHECK_EQ(fake[0x4C / 4], 1u);
  CHECK_EQ(fake[0x38 / 4], 2u);
  CHECK_EQ(fake[0x24 / 4], 0u); // the hole stayed a hole
}
