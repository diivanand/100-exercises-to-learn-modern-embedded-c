// Solution -- 01.04 Designated initialisers: order is not a contract

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
  // Each value is attached to a NAME, not a position. Reorder the struct,
  // insert a member, remove one -- this initialiser stays correct or stops
  // compiling. The positional version just went quietly wrong.
  return (struct uart_config){
      .baud = 115200,
      .data_bits = 8,
      .parity = 'N',
      .stop_bits = 1,
      // .flow_control not named: zeroed, i.e. false (C17 6.7.9p19)
  };
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
  // Index designators pin each string to ITS enumerator. A new enumerator
  // without a new string leaves a visible NULL hole instead of shifting
  // every later name onto the wrong meaning.
  static const char *const NAMES[UART_ERROR_COUNT] = {
      [UART_OK] = "ok",           [UART_OVERRUN] = "overrun",
      [UART_FRAMING] = "framing", [UART_PARITY] = "parity",
      [UART_NOISE] = "noise",
  };
  // The cast makes one comparison cover both ends: a negative value wraps
  // to a huge unsigned one and fails the same bound.
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
