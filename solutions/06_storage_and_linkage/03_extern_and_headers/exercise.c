// Solution -- 06.03 Declarations, definitions and extern

#include <mect/mect.h>

#include <stdint.h>

// ---------------------------------------------------------------------------
// power.h -- declares the counter so any file may reference it
// ---------------------------------------------------------------------------
extern uint32_t wake_count; // a DECLARATION: type and name, no storage
void power_record_wake(void);

// ---------------------------------------------------------------------------
// logger.h
// ---------------------------------------------------------------------------
uint32_t logger_wakes_logged(void);

// ---------------------------------------------------------------------------
// power.c -- the ONE definition
// ---------------------------------------------------------------------------
uint32_t wake_count = 0; // the DEFINITION: this file owns the storage

void power_record_wake(void) {
  ++wake_count;
}

// ---------------------------------------------------------------------------
// logger.c -- refers to the same object through the declaration
// ---------------------------------------------------------------------------
uint32_t logger_wakes_logged(void) {
  return wake_count;
}

TEST("both modules see one counter") {
  power_record_wake();
  power_record_wake();
  power_record_wake();
  CHECK_EQ(logger_wakes_logged(), 3u);
}

TEST("the counter keeps counting across tests") {
  power_record_wake();
  CHECK_EQ(logger_wakes_logged(), 4u); // static duration: 3 from the test above
}
