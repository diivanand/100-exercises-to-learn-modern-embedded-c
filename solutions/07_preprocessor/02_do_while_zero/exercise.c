// Solution -- 07.02 Multi-statement macros and do { } while (0)

#include <mect/mect.h>

enum fault_code { FAULT_NONE, FAULT_OVER_TEMP, FAULT_UNDER_VOLT };

static unsigned fault_count = 0;
static enum fault_code last_fault = FAULT_NONE;
static unsigned ok_count = 0;

// do { } while (0) packages the two statements as ONE statement that still
// demands its trailing semicolon -- so the macro call parses exactly like a
// function call in every position, if/else included (CERT PRE10-C).
#define RECORD_FAULT(code)                                                               \
  do {                                                                                   \
    fault_count++;                                                                       \
    last_fault = (code);                                                                 \
  } while (0)

static void reset_counters(void) {
  fault_count = 0;
  last_fault = FAULT_NONE;
  ok_count = 0;
}

static void check_temperature(int temp_c) {
  if (temp_c > 85)
    RECORD_FAULT(FAULT_OVER_TEMP);
  else
    ok_count++;
}

static void check_supply(int millivolts) {
  if (millivolts < 3000)
    RECORD_FAULT(FAULT_UNDER_VOLT);
  else
    ok_count++;
}

TEST("a fault is recorded with its code") {
  reset_counters();
  check_temperature(90);
  CHECK_EQ(fault_count, 1u);
  CHECK_EQ((int)last_fault, (int)FAULT_OVER_TEMP);
  CHECK_EQ(ok_count, 0u);
}

TEST("a healthy reading takes the else branch") {
  reset_counters();
  check_temperature(25);
  CHECK_EQ(fault_count, 0u);
  CHECK_EQ(ok_count, 1u);
}

TEST("a mixed sequence keeps both counters honest") {
  reset_counters();
  check_temperature(25);  // ok
  check_supply(2800);     // fault
  check_temperature(100); // fault
  check_supply(3300);     // ok
  CHECK_EQ(fault_count, 2u);
  CHECK_EQ(ok_count, 2u);
  CHECK_EQ((int)last_fault, (int)FAULT_OVER_TEMP);
}
