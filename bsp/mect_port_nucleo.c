// The NUCLEO side of the mect harness: the port function feeds USART2, and
// main() runs the tests once and parks the core. `./mec flash` watches the
// serial port for the summary line.

#include <mect/mect.h>

#include "bsp.h"
#include "l476_regs.h"

void mect_port_putc(char c) {
  bsp_uart_putc(c);
}

int main(void) {
  (void)mect_run_all();
  for (;;) {
    wait_for_interrupt();
  }
}
