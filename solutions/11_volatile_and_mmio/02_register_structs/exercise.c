// Solution -- 11.02 A peripheral is a struct at an address

#include <mect/mect.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// --- the register block, per RM0351 ------------------------------------------
// Declaration order IS the memory map: each volatile uint32_t advances the
// offset by 4, and a gap in the map must be spelled out as a reserved
// member. AFR is the one that catches people: the manual documents AFRL
// (0x20) and AFRH (0x24) as two registers; an array of two is the honest
// spelling, and indexing it with pin / 8 falls out naturally.

struct gpio_regs {
  volatile uint32_t MODER;   // 0x00  RM0351 8.5.1
  volatile uint32_t OTYPER;  // 0x04  RM0351 8.5.2
  volatile uint32_t OSPEEDR; // 0x08  RM0351 8.5.3
  volatile uint32_t PUPDR;   // 0x0C  RM0351 8.5.4
  volatile uint32_t IDR;     // 0x10  RM0351 8.5.5
  volatile uint32_t ODR;     // 0x14  RM0351 8.5.6
  volatile uint32_t BSRR;    // 0x18  RM0351 8.5.7
  volatile uint32_t LCKR;    // 0x1C  RM0351 8.5.8
  volatile uint32_t AFR[2];  // 0x20  RM0351 8.5.9/8.5.10 (AFRL, AFRH)
  volatile uint32_t BRR;     // 0x28  RM0351 8.5.11
};

// An excerpt of the RCC map, which -- unlike GPIO -- has holes in it. The
// manual lists nothing at 0x24, 0x34 or 0x44; the struct must still account
// for every byte, so reserved members hold the places.
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
  uint32_t reserved0;            // 0x24  (no register here)
  volatile uint32_t AHB1RSTR;    // 0x28  RM0351 6.4.10
  volatile uint32_t AHB2RSTR;    // 0x2C  RM0351 6.4.11
  volatile uint32_t AHB3RSTR;    // 0x30  RM0351 6.4.12
  uint32_t reserved1;            // 0x34
  volatile uint32_t APB1RSTR1;   // 0x38  RM0351 6.4.13
  volatile uint32_t APB1RSTR2;   // 0x3C  RM0351 6.4.14
  volatile uint32_t APB2RSTR;    // 0x40  RM0351 6.4.15
  uint32_t reserved2;            // 0x44
  volatile uint32_t AHB1ENR;     // 0x48  RM0351 6.4.16
  volatile uint32_t AHB2ENR;     // 0x4C  RM0351 6.4.17
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
