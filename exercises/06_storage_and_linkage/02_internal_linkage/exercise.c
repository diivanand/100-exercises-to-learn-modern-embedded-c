// =============================================================================
//  06.02 -- Internal linkage: static is the module boundary
// =============================================================================
//
//  A name at file scope has LINKAGE -- a rule for whether the same name in
//  another translation unit refers to the same thing (Effective C ch. 2,
//  "Linkage"):
//
//    external   the default. Visible to the whole program; any file that
//               declares the name can read it, write it, call it.
//    internal   `static` at file scope. The name exists only in this file.
//
//  C has no `private`, no modules, no namespaces. `static` is all three, and
//  the rule that follows is blunt: EVERYTHING that is not in the module's
//  header should be static. (C++ refugees: an anonymous namespace, one
//  keyword cheaper.) Three separate payoffs:
//
//   - INVARIANTS SURVIVE. A module's state usually has relationships -- here,
//     `ns_per_bit` must always match `current_baud`. If the variables have
//     external linkage, any file in the program can write one without the
//     other. Nothing stops them; C across translation units barely even
//     type-checks (06.03 shows how bad that gets).
//   - NO COLLISIONS. Every firmware code base has six functions that want to
//     be called `init` or `checksum`. With internal linkage they coexist;
//     with external linkage the linker stops you at best, or -- with C's
//     permissive past -- quietly picks one at worst.
//   - THE LINKER CAN DISCARD MORE. With -ffunction-sections and
//     --gc-sections (the target build in chapter 15 uses both), unreferenced
//     statics are provably dead and get dropped from the image. An external
//     symbol might be referenced by a file not linked yet; it stays.
//
//  The starter below is the invariant failure, live. The uart module owns
//  two file-scope variables with EXTERNAL linkage, and a colleague writing
//  app.c found them: their "reconfigure" pokes `current_baud` directly --
//  it was reachable, it compiled, it even half-works. The derived
//  `ns_per_bit` is now stale, and the second test can tell.
//
//  (In this course a "module" is a banner-commented section of one file, so
//  the poke compiles here either way. In a real tree, once the variables are
//  static, app.c's direct write becomes the compile error it always
//  deserved to be. The lesson is the same.)
//
//  TASK
//    Fix app_reconfigure_for_gps to go through uart_set_baud, and give the
//    uart module's internals internal linkage so the next colleague cannot
//    do this from another file. Do not change the tests.
//
//  RUN IT
//    ./mec test 06_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// ---------------------------------------------------------------------------
// uart.h -- the module's whole public surface
// ---------------------------------------------------------------------------
void uart_set_baud(uint32_t baud);
uint32_t uart_byte_time_ns(void);

// ---------------------------------------------------------------------------
// uart.c -- the module
// ---------------------------------------------------------------------------

// TODO: external linkage -- these are reachable from every file in the
// program, and app.c below has noticed.
uint32_t current_baud;
uint32_t ns_per_bit;

void uart_set_baud(uint32_t baud) {
  if (baud == 0) {
    return; // refuse nonsense rather than divide by it
  }
  current_baud = baud;
  ns_per_bit = 1000000000u / baud;
}

uint32_t uart_byte_time_ns(void) {
  // 8N1: start bit + 8 data + stop bit = 10 bit times per byte.
  return ns_per_bit * 10u;
}

// ---------------------------------------------------------------------------
// app.c -- a colleague's code, reconfiguring the link for a GPS module
// ---------------------------------------------------------------------------

void app_reconfigure_for_gps(void) {
  // TODO: reaches straight past the API and updates half of the module's
  // state. ns_per_bit is now stale.
  current_baud = 9600;
}

TEST("timing follows the configured baud rate") {
  uart_set_baud(115200);
  CHECK_EQ(uart_byte_time_ns(), 86800u); // 1e9/115200 = 8680 ns, x10 bits
}

TEST("reconfiguring through the app keeps timing consistent") {
  uart_set_baud(115200);
  app_reconfigure_for_gps();
  CHECK_EQ(uart_byte_time_ns(), 1041660u); // 1e9/9600 = 104166 ns, x10 bits
}

TEST("a zero baud request is refused, not divided by") {
  uart_set_baud(115200);
  uart_set_baud(0);
  CHECK_EQ(uart_byte_time_ns(), 86800u);
}
