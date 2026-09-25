// =============================================================================
//  15.02 -- Startup: the three jobs before main()
// =============================================================================
//
//  Between the reset vector and main() stand three loops, and today you
//  write them. The board support startup (bsp/startup_stm32l476.c) declares
//  its versions `weak`; the strong definitions in THIS file replace them at
//  link time -- and the ones you have been given are empty.
//
//  The three jobs, in order (each is a handful of lines):
//
//  1. COPY .data. Initialised globals live at RAM addresses, but RAM forgets;
//     their initial values were placed in flash by the linker (15.01's
//     `AT> FLASH`). Copy word by word from `_sidata` (flash) to
//     `_sdata`..`_edata` (RAM). Until this runs, every initialised global --
//     including the C library's own bookkeeping -- reads stale memory.
//
//  2. ZERO .bss. The C standard promises static-duration objects without an
//     initialiser start at zero (Effective C ch. 2, storage duration). On a
//     desktop the OS zeroes fresh pages; here, nobody zeroes an SRAM cell
//     but you. Zero `_sbss`..`_ebss`.
//
//  3. WALK .init_array. The compiler collects addresses of functions that
//     must run before main -- the mect harness registers every TEST() below
//     through one -- between `__init_array_start` and `__init_array_end`.
//     Call each in order.
//
//  WHAT YOU WILL SEE FIRST. With all three empty, the harness has no tests
//  (nothing registered) and its own printing machinery stands on
//  uninitialised .data -- so expect either "[mect] no tests were registered"
//  or, on a cold power-up, TOTAL SILENCE. `./mec flash` reports both
//  honestly. Fix job 3 first to get tests running, and jobs 1 and 2 to make
//  them pass.
//
//  ONE SUBTLETY THE TESTS DEFEND AGAINST. SRAM retains its contents across a
//  warm reset. A previous PASSING run leaves correct values behind, so a
//  broken copy loop could coast on stale data and look green. Every test
//  below therefore SCRAMBLES what it checked before finishing -- the next
//  boot must earn its values again. Remember the trick; it is a general one
//  for testing anything reset-shaped.
//
//  RUN IT
//    ./mec flash 15_02
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// Section boundaries, defined by the linker script (see 15.01).
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

typedef void (*init_fn)(void);
extern init_fn __init_array_start[], __init_array_end[];

// --- the three jobs (yours) ---------------------------------------------------

void startup_copy_data(void) {
  // TODO: copy [_sidata ...] in flash to [_sdata .. _edata) in RAM.
}

void startup_zero_bss(void) {
  // TODO: zero [_sbss .. _ebss).
}

void startup_call_init_array(void) {
  // TODO: call every function between __init_array_start and __init_array_end.
}

// --- the evidence ---------------------------------------------------------------

static uint32_t data_markers[4] = {0x11AA22BBu, 0x33CC44DDu, 0x55EE66FFu, 0xC0FFEE99u};
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
  // _edata and _sbss bound the two jobs; if your loops used <= or mixed up
  // byte and word counts, the section AFTER the one you processed took the
  // damage. The linker placed these next to each other, so each section is
  // the other's canary. (Both regions were verified above; this test exists
  // to hold the thought.)
  CHECK((uintptr_t)&_sdata <= (uintptr_t)&_edata);
  CHECK((uintptr_t)&_sbss <= (uintptr_t)&_ebss);
}
