// Solution -- 01.06 Enums: named states, wire values, and the switch

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// Explicit values: these appear in flash logs read by tools that outlive any
// one firmware build, so each enumerator's value is a published contract,
// not an accident of listing order.
enum wake_source {
  WAKE_RESET = 0x01,
  WAKE_TIMER = 0x02,
  WAKE_PIN = 0x04,
  WAKE_WATCHDOG = 0x08, // added for the brown-out investigation
};

uint8_t wake_source_to_wire(enum wake_source s) {
  // No default: with -Wswitch (part of -Wall), a switch over an enum that
  // omits an enumerator is a diagnostic. Add a `default:` and you buy
  // silence at the price of that safety net -- every FUTURE enumerator
  // becomes a silent fall-through instead of a build break. Leave enum
  // switches exhaustive and let the compiler mind the list.
  switch (s) {
  case WAKE_RESET:
    return 0x01;
  case WAKE_TIMER:
    return 0x02;
  case WAKE_PIN:
    return 0x04;
  case WAKE_WATCHDOG:
    return 0x08;
  }
  return 0; // unreachable for valid inputs; 09.05 discusses asserting here
}

bool wake_source_from_wire(uint8_t byte, enum wake_source *out) {
  // The boundary check. An enum variable is not magic: it is an integer
  // type, and a cast will happily store 0xFF in it. Validation happens
  // HERE, where bytes enter the system, or nowhere.
  switch (byte) {
  case 0x01:
    *out = WAKE_RESET;
    return true;
  case 0x02:
    *out = WAKE_TIMER;
    return true;
  case 0x04:
    *out = WAKE_PIN;
    return true;
  case 0x08:
    *out = WAKE_WATCHDOG;
    return true;
  default:
    return false; // over the wire, ANY byte can arrive; a default belongs
  }
}

TEST("every wake source has a wire value") {
  CHECK_EQ(wake_source_to_wire(WAKE_RESET), 0x01u);
  CHECK_EQ(wake_source_to_wire(WAKE_TIMER), 0x02u);
  CHECK_EQ(wake_source_to_wire(WAKE_PIN), 0x04u);
  CHECK_EQ(wake_source_to_wire(WAKE_WATCHDOG), 0x08u);
}

TEST("decoding accepts exactly the published values") {
  enum wake_source s;
  CHECK(wake_source_from_wire(0x02, &s));
  CHECK_EQ(s, WAKE_TIMER);
  CHECK(wake_source_from_wire(0x08, &s));
  CHECK_EQ(s, WAKE_WATCHDOG);
}

TEST("decoding rejects everything else") {
  enum wake_source s = WAKE_RESET;
  CHECK_FALSE(wake_source_from_wire(0x00, &s));
  CHECK_FALSE(wake_source_from_wire(0x03, &s)); // two bits at once
  CHECK_FALSE(wake_source_from_wire(0xFF, &s));
  CHECK_EQ(s, WAKE_RESET); // a rejected byte must not modify the output
}
