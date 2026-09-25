// The host-side entry point and port. Every host exercise links against the
// static library built from mect.c plus this file, so main() is compiled once
// for the whole project rather than once per exercise.
//
// On the NUCLEO target this file is replaced by bsp/mect_port_nucleo.c, where
// mect_port_putc() feeds USART2 and main() lives in the board support code.

#include "mect.h"

#include <stdio.h>

void mect_port_putc(char c) {
  putchar(c);
}

int main(void) {
  const int failed = mect_run_all();
  return failed == 0 ? 0 : 1;
}
