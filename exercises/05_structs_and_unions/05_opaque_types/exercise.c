// =============================================================================
//  05.05 -- Opaque types: the header is a promise, the struct is a secret
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- the callers below use
//  two accessors that do not exist yet.
//
//  Every struct so far has been public. The cost arrives with the first
//  caller that reaches in: once anyone can write `u->baud = 7;`, every
//  invariant the module maintains -- a baud rate that was actually
//  validated, counters that match reality -- depends on every caller's good
//  behaviour, forever. C's fix is older than object-orientation's name for
//  it. Put ONLY a declaration in the header:
//
//      struct uart;                            // a name, no body: INCOMPLETE
//      bool uart_configure(struct uart *u, uint32_t baud);
//
//  and keep the definition inside the one .c file that implements it.
//  Callers can hold and pass `struct uart *` freely, but `u->baud` in a
//  caller is not merely rude -- it DOES NOT COMPILE ("incomplete type"). The
//  module boundary is enforced by the compiler, not by a comment. (Effective
//  C 2nd ed. ch. 10, "Opaque Types". You already use the pattern daily:
//  newlib's FILE, every RTOS's task handle.)
//
//  The embedded twist is where instances LIVE. Desktop C would malloc one in
//  uart_create(); there is no malloc here (chapter 08 says why). Instead the
//  module owns a fixed pool -- this chip has exactly two UARTs, so exactly
//  two slots -- and hands out CLAIMS on them: uart_claim(n) returns port n's
//  handle once, NULL while it is taken or if n is out of range, and
//  uart_release() returns it. Refusal-by-NULL makes double-claim bugs loud
//  at the claim site instead of quiet at the register level.
//
//  This file plays three roles, top to bottom: uart.h (the interface),
//  main.c (the callers -- the tests), uart.c (the implementation). In a real
//  project those are three files; the banners mark the boundaries, and the
//  rule of the exercise is that callers use only what the uart.h section
//  declares.
//
//  TASK
//    The callers need to OBSERVE a port -- its configured baud, its bytes
//    sent -- and the interface has no way to ask. Add the two accessors the
//    tests call: declarations in the uart.h section, definitions in the
//    uart.c section. Note what you never write: a setter.
//
//  RUN IT
//    ./mec test 05_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// uart.h -- the interface. Callers see this and nothing else.
// ---------------------------------------------------------------------------

struct uart; // incomplete: a name callers can hold, not open

struct uart *uart_claim(uint8_t index); // NULL: out of range, or taken
void uart_release(struct uart *u);
bool uart_configure(struct uart *u, uint32_t baud); // false: rejected
void uart_send(struct uart *u, uint8_t byte);

// TODO: declare the two observers the callers below need --
//
//   uint32_t uart_baud(const struct uart *u);
//   size_t uart_bytes_sent(const struct uart *u);
//
// -- and define them down in the uart.c section. Two lines each: the whole
// price of keeping the struct private.

// ---------------------------------------------------------------------------
// main.c -- the callers. Only the uart.h section above is theirs to use.
// ---------------------------------------------------------------------------

TEST("claim hands out each port once") {
  struct uart *a = uart_claim(0);
  REQUIRE(a != NULL);
  CHECK(uart_claim(0) == NULL); // taken until released
  struct uart *b = uart_claim(1);
  REQUIRE(b != NULL);
  CHECK(a != b);
  CHECK(uart_claim(2) == NULL); // the chip has two
  uart_release(a);
  uart_release(b);
}

TEST("configuration is validated, and observable") {
  struct uart *u = uart_claim(0);
  REQUIRE(u != NULL);
  CHECK(uart_configure(u, 115200));
  CHECK_EQ(uart_baud(u), 115200u);
  CHECK_FALSE(uart_configure(u, 0)); // rejected...
  CHECK_EQ(uart_baud(u), 115200u);   // ...and the old setting survives

  // Try uncommenting the next line: "incomplete definition of type
  // 'struct uart'". The compiler is holding the module boundary shut.
  // CHECK_EQ(u->baud, 115200u);
  uart_release(u);
}

TEST("traffic is counted, and a released port comes back fresh") {
  struct uart *u = uart_claim(1);
  REQUIRE(u != NULL);
  uart_send(u, 0x55);
  uart_send(u, 0xAA);
  CHECK_EQ(uart_bytes_sent(u), 2u);
  uart_release(u);

  struct uart *again = uart_claim(1);
  REQUIRE(again != NULL);
  CHECK_EQ(uart_bytes_sent(again), 0u);
  uart_release(again);
}

// ---------------------------------------------------------------------------
// uart.c -- the implementation: the only lines in the program entitled to
// say `->baud`.
// ---------------------------------------------------------------------------

struct uart {
  bool claimed;
  uint32_t baud;
  size_t bytes_sent;
};

static struct uart uart_pool[2]; // exactly as many as the chip has

struct uart *uart_claim(uint8_t index) {
  if ((size_t)index >= sizeof uart_pool / sizeof uart_pool[0]) {
    return NULL;
  }
  struct uart *u = &uart_pool[index];
  if (u->claimed) {
    return NULL;
  }
  *u = (struct uart){.claimed = true}; // every claim starts from clean state
  return u;
}

void uart_release(struct uart *u) {
  u->claimed = false;
}

bool uart_configure(struct uart *u, uint32_t baud) {
  if (baud == 0) {
    return false;
  }
  u->baud = baud;
  return true;
}

void uart_send(struct uart *u, uint8_t byte) {
  // A real driver would write `byte` to a TX register (chapter 11); the
  // caller-visible effect this exercise models is the count.
  (void)byte;
  u->bytes_sent++;
}
