// Solution -- 15.01 The memory map is yours now

#include <mect/mect.h>

#include <stdint.h>

// The linker script defines these SYMBOLS. A symbol is an address with a
// name -- there is no storage behind these beyond what the sections
// themselves are. Taking `&_estack` asks "what address did the linker give
// this name?"; reading `_estack` would read whatever bytes happen to live
// there. The uint32_t type is a polite fiction; only the address is real.
extern uint32_t _estack, _sdata, _edata, _sidata, _sbss, _ebss;

void Reset_Handler(void);

enum {
  FLASH_BASE = 0x08000000u,
  FLASH_SIZE = 1024u * 1024u,
  SRAM1_BASE = 0x20000000u,
  SRAM1_SIZE = 96u * 1024u,
};

TEST("the stack starts at the top of SRAM1") {
  // 0x20000000 + 96 KB = 0x20018000. The starter's script said 64 KB, which
  // boots and runs -- and silently donates a third of your RAM to nothing.
  CHECK_EQ((uint32_t)(uintptr_t)&_estack, SRAM1_BASE + SRAM1_SIZE);
}

TEST("the vector table is where the core will look for it") {
  // With BOOT0 = 0, flash is aliased at address 0 and the core reads its
  // initial SP from offset 0 and initial PC from offset 4. Entry 1 must be
  // Reset_Handler -- with bit 0 set, because a Cortex-M executes only Thumb
  // code and the address advertises it. C function pointers on this target
  // carry that bit already.
  const volatile uint32_t *vectors = (const volatile uint32_t *)FLASH_BASE;
  CHECK_EQ(vectors[0], SRAM1_BASE + SRAM1_SIZE);
  CHECK_EQ(vectors[1], (uint32_t)(uintptr_t)&Reset_Handler);
  CHECK_EQ(vectors[1] & 1u, 1u); // the Thumb bit
}

TEST(".data: values in flash, addresses in RAM") {
  const uint32_t sdata = (uint32_t)(uintptr_t)&_sdata;
  const uint32_t edata = (uint32_t)(uintptr_t)&_edata;
  const uint32_t sidata = (uint32_t)(uintptr_t)&_sidata;

  // VMA in RAM: the addresses the program uses.
  CHECK(sdata >= SRAM1_BASE);
  CHECK(edata <= SRAM1_BASE + SRAM1_SIZE);
  // LMA in flash: where the initial values survive power-off.
  CHECK(sidata >= FLASH_BASE);
  CHECK(sidata < FLASH_BASE + FLASH_SIZE);
}

TEST(".bss sits after .data, both word-aligned") {
  const uint32_t edata = (uint32_t)(uintptr_t)&_edata;
  const uint32_t sbss = (uint32_t)(uintptr_t)&_sbss;
  const uint32_t ebss = (uint32_t)(uintptr_t)&_ebss;

  CHECK(sbss >= edata);
  CHECK(ebss >= sbss);
  CHECK_EQ((uint32_t)(uintptr_t)&_sdata % 4u, 0u);
  CHECK_EQ(sbss % 4u, 0u);
}

TEST("initialised data really was initialised (the script's AT> at work)") {
  static uint32_t marker = 0xC0FFEE42u;
  CHECK_EQ(marker, 0xC0FFEE42u);
  marker = 0; // scramble: a stale value must not fake a pass on reboot
}
