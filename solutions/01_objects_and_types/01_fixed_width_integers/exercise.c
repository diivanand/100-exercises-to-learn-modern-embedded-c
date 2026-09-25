// Solution -- 01.01 Fixed-width integers: say what you can hold

#include <mect/mect.h>

#include <stdint.h>

// uint32_t: wraps after 49.7 days, which the header comment of the module
// can say out loud. `unsigned short` wrapped after 65 seconds and said
// nothing.
struct uptime {
  uint32_t ms;
};

void uptime_add(struct uptime *u, uint32_t ms) {
  u->ms += ms;
}

uint32_t uptime_get(const struct uptime *u) {
  return u->ms;
}

// The sensor byte is 0..255, so the field must be unsigned. The starter's
// cast to signed char was not a fix, it was a confession: a cast silences
// the compiler WITHOUT changing what the type can hold.
struct sensor {
  uint8_t last;
};

void sensor_store(struct sensor *s, uint8_t raw) {
  s->last = raw;
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
