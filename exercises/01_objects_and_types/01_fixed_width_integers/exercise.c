// =============================================================================
//  01.01 -- Fixed-width integers: say what you can hold
// =============================================================================
//
//  K&R taught you `int`, `short`, `long`. What it could not tell you in 1988
//  is how little those names promise: the standard guarantees int AT LEAST
//  16 bits, long at least 32, and stops there. An `int` is 16 bits on an
//  MSP430, 32 on a Cortex-M4 -- and firmware moves between parts more often
//  than anyone plans to. (Effective C ch. 3, "Integer Ranges"; CERT INT00-C:
//  understand the data model.)
//
//  C99's <stdint.h> ended the guessing:
//
//      uint8_t, int16_t, uint32_t...   exactly that many bits
//      uint_least8_t, int_least32_t    the smallest type with at least that
//      uint_fast8_t                    the FASTEST type with at least that
//      UINT32_C(0xFFFF0000)            a literal with a guaranteed-wide type
//
//  The exact-width types are technically optional, but every platform this
//  course cares about -- and every platform with 8-bit bytes and two's
//  complement, which since C23 is the only kind C admits exists -- has them.
//
//  The working rules of thumb, which this course uses throughout:
//
//   - STORAGE gets an exact width. A struct field, a register value, a
//     protocol byte, an array that multiplies RAM: name the width you mean.
//   - Loop-local ARITHMETIC can use int/unsigned/size_t. Promotions turn
//     small types into int inside expressions anyway (02.01 has the scars).
//   - A CAST IS NOT A FIX. `(signed char)raw` makes a warning go away; it
//     does not make 200 fit in -128..127. If the compiler objects to a
//     store, first ask whether the DESTINATION type is the right size.
//
//  Below, a device uptime counter that a colleague sized for the bench test
//  instead of the field, and a sensor cache with a sign problem.
//
//  TASK
//    Fix the two struct definitions (and remove the cast that was covering
//    for one of them). Do not change the tests.
//
//  RUN IT
//    ./mec test 01_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// Milliseconds since boot.
struct uptime {
  unsigned short ms; // TODO: 65,535 ms is 65 seconds. Field devices run for months.
};

void uptime_add(struct uptime *u, uint32_t ms) {
  u->ms = (unsigned short)(u->ms + ms);
}

uint32_t uptime_get(const struct uptime *u) {
  return u->ms;
}

// The last raw byte read from a sensor (0..255).
struct sensor {
  signed char last; // TODO: 0..255 does not fit; the cast below was the cover-up
};

void sensor_store(struct sensor *s, uint8_t raw) {
  s->last = (signed char)raw;
}

int sensor_last(const struct sensor *s) {
  return s->last;
}

TEST("uptime survives more than 65 seconds") {
  struct uptime u = {0};
  uptime_add(&u, 40000);
  uptime_add(&u, 40000);
  CHECK_EQ(uptime_get(&u), 80000u); // unsigned short wrapped to 14464 here

  struct uptime day = {0};
  uptime_add(&day, 86400000); // one day of milliseconds
  CHECK_EQ(uptime_get(&day), 86400000u);
}

TEST("sensor readings above 127 stay positive") {
  struct sensor s = {0};
  sensor_store(&s, 200);
  CHECK_EQ(sensor_last(&s), 200); // signed char made this -56

  sensor_store(&s, 127); // the biggest value bench testing ever produced
  CHECK_EQ(sensor_last(&s), 127);
}

TEST("what the language actually guarantees") {
  // Exact-width types are exact everywhere they exist at all.
  CHECK_EQ(sizeof(uint32_t), 4u);
  CHECK_EQ(sizeof(uint8_t), 1u);
  // Plain int is only promised 16 bits. On this host it is wider -- which is
  // exactly how code that assumes 32 gets written.
  CHECK(sizeof(int) >= 2u);
}
