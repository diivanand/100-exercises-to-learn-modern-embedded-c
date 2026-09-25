// Solution -- 15.02 Startup: the three jobs before main()

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

typedef void (*init_fn)(void);
extern init_fn __init_array_start[], __init_array_end[];

// These strong definitions replace the weak ones in bsp/startup_stm32l476.c.
// They are, deliberately, the same code -- the point of the exercise is that
// you can now read that file and find no magic left in it.

void startup_copy_data(void) {
  // Word-at-a-time is fine: the linker script aligns _sdata and _edata to 4.
  // The flash-side pointer walks in lockstep with the RAM-side one.
  const uint32_t *src = &_sidata;
  for (uint32_t *dst = &_sdata; dst < &_edata; ++dst) {
    *dst = *src++;
  }
}

void startup_zero_bss(void) {
  for (uint32_t *dst = &_sbss; dst < &_ebss; ++dst) {
    *dst = 0;
  }
  // (Production startups often use memset here. Ours must not: memset's own
  // code is fine, but the C library is entitled to keep state in .data/.bss
  // -- calling into it while those are half-built is a bootstrap paradox.)
}

void startup_call_init_array(void) {
  // __init_array_start/end are declared as ARRAYS so the pointer arithmetic
  // is natural; the linker made them bound a table of function pointers.
  for (init_fn *fn = __init_array_start; fn < __init_array_end; ++fn) {
    (*fn)();
  }
}

static uint32_t data_markers[4] = {0x11AA22BBu, 0x33CC44DDu, 0x55EE66FFu,
                                   0xC0FFEE99u};
static uint32_t bss_field[256]; // no initialiser: the language says all zero

TEST(".data: initialised globals hold their written values") {
  CHECK_EQ(data_markers[0], 0x11AA22BBu);
  CHECK_EQ(data_markers[1], 0x33CC44DDu);
  CHECK_EQ(data_markers[2], 0x55EE66FFu);
  CHECK_EQ(data_markers[3], 0xC0FFEE99u);

  // Scramble: stale RAM must not fake a pass on the next boot.
  for (size_t i = 0; i < 4; ++i) {
    data_markers[i] = 0xDEADDEADu;
  }
}

TEST(".bss: uninitialised globals are all zero") {
  uint32_t accumulated = 0;
  for (size_t i = 0; i < 256; ++i) {
    accumulated |= bss_field[i];
  }
  CHECK_EQ(accumulated, 0u);

  // Scramble, same reason.
  for (size_t i = 0; i < 256; ++i) {
    bss_field[i] = 0xA5A5A5A5u;
  }
}

TEST("the copy and the zero stayed inside their fences") {
  CHECK((uintptr_t)&_sdata <= (uintptr_t)&_edata);
  CHECK((uintptr_t)&_sbss <= (uintptr_t)&_ebss);
}
