// Solution -- 06.04 .data, .bss, .rodata: where globals actually live

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
  // Copy .data's initial image from "flash" to its run-time home. The
  // source is the LOAD image; copying RAM to RAM would move garbage.
  for (size_t i = 0; i < DATA_SIZE; ++i) {
    ram[DATA_START + i] = data_load_image[i];
  }
  // Zero .bss -- its own region, all of it, and nothing else. The C standard
  // promises uninitialised statics are zero; on a real part THIS LOOP is
  // that promise (15.02 makes you keep it on real hardware).
  for (size_t i = 0; i < BSS_SIZE; ++i) {
    ram[BSS_START + i] = 0;
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
