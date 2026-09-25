// Solution -- 05.05 Opaque types: the header is a promise, the struct is a secret

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

// The two observers, declared. Note what was NOT added: a setter. Reading
// out state is cheap to grant; arbitrary mutation is what the opacity is
// there to prevent.
uint32_t uart_baud(const struct uart *u);
size_t uart_bytes_sent(const struct uart *u);

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

uint32_t uart_baud(const struct uart *u) {
  return u->baud;
}

size_t uart_bytes_sent(const struct uart *u) {
  return u->bytes_sent;
}
