// Solution -- 06.02 Internal linkage: static is the module boundary

#include <mect/mect.h>

#include <stdint.h>

// ---------------------------------------------------------------------------
// uart.h -- the module's whole public surface
// ---------------------------------------------------------------------------
void uart_set_baud(uint32_t baud);
uint32_t uart_byte_time_ns(void);

// ---------------------------------------------------------------------------
// uart.c -- the module. Everything not in the header is static.
// ---------------------------------------------------------------------------

// Internal linkage: these names do not exist outside this "file". No other
// module can read them, write them, or collide with them. The invariant --
// ns_per_bit always matches current_baud -- now has exactly one door, and
// uart_set_baud is standing in it.
static uint32_t current_baud;
static uint32_t ns_per_bit;

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
  // Through the front door. The module keeps its derived state consistent;
  // the colleague no longer needs to know ns_per_bit exists. (With the
  // internals static, the old direct write would not even compile from a
  // real separate app.c.)
  uart_set_baud(9600);
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
