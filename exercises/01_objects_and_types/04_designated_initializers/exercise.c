// =============================================================================
//  01.04 -- Designated initialisers: order is not a contract
// =============================================================================
//
//  C99 lets an initialiser name its target instead of relying on position --
//  for struct members AND for array indices:
//
//      struct uart_config cfg = {.baud = 115200, .parity = 'N'};
//      const char *const NAMES[] = {[UART_OK] = "ok", [UART_NOISE] = "noise"};
//
//  (C17 6.7.9. C++ programmers: C++20 adopted the struct half only, and even
//  then insists on declaration order. The array-index form is C's alone.)
//
//  Two things make this more than convenience:
//
//   1. UNNAMED MEMBERS ARE ZEROED -- the same 6.7.9p19 rule as 01.03. A
//      designated initialiser is therefore a complete initialiser: name what
//      is non-zero, and nothing is left as garbage.
//
//   2. THE INITIALISER SURVIVES CHANGE. A positional initialiser encodes the
//      declaration order of members it does not even mention. Insert one
//      member -- or one enumerator -- and every later value lands one slot
//      off, silently. With designators the same edit either stays correct or
//      fails to compile. For lookup tables indexed by an enum, index
//      designators are the difference between "the table and the enum cannot
//      drift" and "grep and pray".
//
//  Both failures are waiting below, in code that was correct when written:
//
//   - `uart_default_config` initialises positionally. The struct's members
//     were later reordered to pack better (chapter 05 explains why) and now
//     a 1 lands in `parity` and the letter 'N' in `stop_bits`. It compiles:
//     both values fit both members. (Had the list also been INCOMPLETE,
//     -Wextra's -Wmissing-field-initializers would at least have grumbled --
//     about the wrong thing. Complete and wrong is fully silent.)
//   - `NAMES` was written when there were four error codes. UART_FRAMING was
//     added in the middle of the enum -- where it belongs logically -- and
//     every name after it now describes the wrong error, in the log files of
//     every field unit.
//
//  TASK
//    Convert both initialisers to designated form (and give UART_FRAMING its
//    name). Do not change the tests.
//
//  RUN IT
//    ./mec test 01_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

struct uart_config {
  uint32_t baud;
  uint8_t data_bits;
  char parity; // 'N', 'E' or 'O'
  uint8_t stop_bits;
  bool flow_control;
};

struct uart_config uart_default_config(void) {
  // TODO: written for an older member order: baud, data_bits, stop_bits,
  // parity, flow_control. Every swapped value still fits its accidental
  // destination, so this still compiles without a murmur.
  return (struct uart_config){115200, 8, 1, 'N', false};
}

enum uart_error {
  UART_OK,
  UART_OVERRUN,
  UART_FRAMING, // added in a later firmware revision
  UART_PARITY,
  UART_NOISE,
  UART_ERROR_COUNT,
};

const char *uart_error_name(enum uart_error e) {
  // TODO: this table predates UART_FRAMING. Every entry after "overrun" now
  // sits one slot early.
  static const char *const NAMES[UART_ERROR_COUNT] = {
      "ok",
      "overrun",
      "parity",
      "noise",
  };
  if ((unsigned)e >= UART_ERROR_COUNT || NAMES[e] == NULL) {
    return "unknown";
  }
  return NAMES[e];
}

TEST("the default config means what the datasheet says") {
  const struct uart_config cfg = uart_default_config();
  CHECK_EQ(cfg.baud, 115200u);
  CHECK_EQ(cfg.data_bits, 8u);
  CHECK_EQ(cfg.parity, 'N');
  CHECK_EQ(cfg.stop_bits, 1u);
  CHECK_EQ(cfg.flow_control, false);
}

TEST("every error code has its own name") {
  CHECK_EQ(uart_error_name(UART_OK), "ok");
  CHECK_EQ(uart_error_name(UART_OVERRUN), "overrun");
  CHECK_EQ(uart_error_name(UART_FRAMING), "framing");
  CHECK_EQ(uart_error_name(UART_PARITY), "parity");
  CHECK_EQ(uart_error_name(UART_NOISE), "noise");
}

TEST("out-of-range codes do not walk off the table") {
  CHECK_EQ(uart_error_name(UART_ERROR_COUNT), "unknown");
  CHECK_EQ(uart_error_name((enum uart_error)99), "unknown");
}
