// =============================================================================
//  06.04 -- .data, .bss, .rodata: where globals actually live
// =============================================================================
//
//  On a desktop, "a global variable exists" is the operating system's
//  problem. On a microcontroller it is yours, and it works like this. The
//  linker sorts every static-duration object into a SECTION, and each
//  section has a different life:
//
//    .rodata   const objects and string literals. Stays in FLASH; never
//              copied anywhere. Costs flash only. This is why lookup
//              tables should be `static const` (01.07): on a 128 KB-RAM,
//              1 MB-flash part like the L476RG, flash is the cheap one.
//
//    .data     initialised, mutable globals.  static uint32_t x = 42;
//              Lives in RAM at run time -- but RAM is garbage at power-up,
//              so the INITIAL VALUES are stored in flash too, and startup
//              code copies them out before main() runs. Costs flash AND RAM.
//
//    .bss      zero-initialised (or uninitialised) globals. No image needed
//              -- startup just zeroes the region. Costs RAM only. (The name
//              is a 1950s assembler directive, "block started by symbol";
//              nobody remembers it, everybody uses it.)
//
//  So `static uint8_t table[4096] = {1};` bloats the flash image by 4 KB and
//  eats 4 KB of RAM, while `= {0}` eats only the RAM: {0} means .bss, and
//  .bss ships no bytes. And the C guarantee that statics start at zero
//  (01.03 leaned on it) is not magic -- it is a loop that somebody wrote,
//  running between reset and main(). In chapter 15 that somebody is you, on
//  real hardware, where getting it wrong means the board boots into
//  garbage. This exercise is the dry run, against a pretend RAM you can
//  inspect.
//
//  Two vocabulary words the linker will use at you in 15.01:
//
//    LMA  load memory address   -- where a section's bytes are STORED
//                                  (.data's image, in flash)
//    VMA  virtual memory address -- where the section LIVES at run time
//                                  (.data itself, in RAM)
//
//  The starter's startup routine has both classic bugs. The copy loop reads
//  from the wrong symbol -- RAM instead of the flash image (LMA/VMA
//  confusion), so .data ends up full of power-up garbage. And the zero loop
//  starts at the wrong base, wiping half of the .data it should have
//  preserved while leaving the top of .bss dirty. Boards have shipped with
//  both.
//
//  (Effective C ch. 2, "Storage Duration"; the linker-script version of this
//  table is RM0351-adjacent reading in 15.01.)
//
//  TASK
//    Fix c_startup so .data is copied from its load image and exactly .bss
//    is zeroed. Do not change the tests.
//
//  RUN IT
//    ./mec test 06_04
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The pretend part's RAM layout, as byte offsets into ram[]:
//
//   [DATA_START, DATA_START+DATA_SIZE)   .data -- initialised globals
//   [BSS_START,  BSS_START+BSS_SIZE)     .bss  -- zero-initialised globals
//   [BSS_START+BSS_SIZE, RAM_SIZE)       untouched (heap/stack territory)
enum {
  DATA_START = 0,
  DATA_SIZE = 12,
  BSS_START = DATA_START + DATA_SIZE,
  BSS_SIZE = 20,
  RAM_SIZE = 40,
};

// The .data section's initial values as stored in flash (its LMA -- load
// memory address). At run time the section lives in RAM (its VMA); somebody
// has to move the bytes, and that somebody is about to be you.
static const uint8_t data_load_image[DATA_SIZE] = {
    0xC0, 0xFF, 0xEE, 0x15, 0x60, 0x0D, 0x0A, 0x11, 0x7E, 0x57, 0xAB, 0x1E,
};

void c_startup(uint8_t *ram) {
  // TODO: copies RAM to RAM. The initial values are in data_load_image --
  // that is the whole reason it exists.
  for (size_t i = 0; i < DATA_SIZE; ++i) {
    ram[DATA_START + i] = ram[BSS_START + i];
  }
  // TODO: zeroes a region of the right SIZE but the wrong BASE: it wipes
  // .data and misses the top of .bss.
  for (size_t i = 0; i < BSS_SIZE; ++i) {
    ram[DATA_START + i] = 0;
  }
}

TEST("after startup, .data holds its initialisers") {
  uint8_t ram[RAM_SIZE];
  memset(ram, 0xAA, sizeof ram); // power-up garbage
  c_startup(ram);
  CHECK_MEM_EQ(ram + DATA_START, data_load_image, DATA_SIZE);
}

TEST("after startup, .bss is all zeros") {
  uint8_t ram[RAM_SIZE];
  memset(ram, 0xAA, sizeof ram);
  c_startup(ram);
  for (size_t i = 0; i < BSS_SIZE; ++i) {
    CHECK_EQ(ram[BSS_START + i], 0u);
  }
}

TEST("startup leaves the rest of RAM alone") {
  uint8_t ram[RAM_SIZE];
  memset(ram, 0xAA, sizeof ram);
  c_startup(ram);
  for (size_t i = BSS_START + BSS_SIZE; i < RAM_SIZE; ++i) {
    CHECK_EQ(ram[i], 0xAAu); // canary territory: not startup's to touch
  }
}
