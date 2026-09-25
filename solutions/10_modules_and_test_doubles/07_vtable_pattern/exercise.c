// Solution -- 10.07 The vtable pattern: many devices, one interface

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- sensor interface ---------------------------------------------------------

enum sensor_status { SENSOR_OK = 0, SENSOR_ERR_NOT_STARTED };

struct sensor_ops {
  enum sensor_status (*start)(void *ctx);
  enum sensor_status (*read)(void *ctx, int32_t *out);
};

struct sensor {
  const struct sensor_ops *ops; // shared per TYPE (lives in .rodata, 07.07)
  void *ctx;                    // owned per INSTANCE
};

// The generic layer: pure dispatch. No statics, no memory of who was
// "last": everything it needs rides in on `s`. That is what lets any
// number of instances coexist -- the property the two-instance tests pin.
static enum sensor_status sensor_start(struct sensor *s) {
  return s->ops->start(s->ctx);
}

static enum sensor_status sensor_read(struct sensor *s, int32_t *out) {
  return s->ops->read(s->ctx, out);
}

// --- backend: thermistor (temperature ramps as the part self-heats) -------------

struct thermistor_ctx {
  int32_t base_centi_c;
  int32_t drift_per_read;
  int32_t reads;
  bool started;
};

static enum sensor_status thermistor_start(void *ctx) {
  struct thermistor_ctx *t = ctx;
  t->reads = 0;
  t->started = true;
  return SENSOR_OK;
}

static enum sensor_status thermistor_read(void *ctx, int32_t *out) {
  struct thermistor_ctx *t = ctx;
  if (!t->started) {
    return SENSOR_ERR_NOT_STARTED;
  }
  *out = t->base_centi_c + t->drift_per_read * t->reads;
  ++t->reads;
  return SENSOR_OK;
}

static const struct sensor_ops thermistor_ops = {thermistor_start, thermistor_read};

// --- backend: quadrature encoder (position advances one detent per read) ---------

struct encoder_ctx {
  int32_t position;
  int32_t detent_step;
  bool started;
};

static enum sensor_status encoder_start(void *ctx) {
  struct encoder_ctx *e = ctx;
  e->started = true;
  return SENSOR_OK;
}

static enum sensor_status encoder_read(void *ctx, int32_t *out) {
  struct encoder_ctx *e = ctx;
  if (!e->started) {
    return SENSOR_ERR_NOT_STARTED;
  }
  *out = e->position;
  e->position += e->detent_step;
  return SENSOR_OK;
}

static const struct sensor_ops encoder_ops = {encoder_start, encoder_read};

// --- tests ----------------------------------------------------------------------

TEST("read before start is refused") {
  struct thermistor_ctx t = {.base_centi_c = 2500, .drift_per_read = 10};
  struct sensor s = {&thermistor_ops, &t};
  int32_t v = -1;
  CHECK_EQ((int)sensor_read(&s, &v), (int)SENSOR_ERR_NOT_STARTED);
  CHECK_EQ(v, -1); // untouched on failure
}

TEST("two thermistors do not share state") {
  struct thermistor_ctx warm = {.base_centi_c = 2500, .drift_per_read = 10};
  struct thermistor_ctx cool = {.base_centi_c = 1800, .drift_per_read = 0};
  struct sensor sa = {&thermistor_ops, &warm};
  struct sensor sb = {&thermistor_ops, &cool};
  CHECK_EQ((int)sensor_start(&sa), (int)SENSOR_OK);
  CHECK_EQ((int)sensor_start(&sb), (int)SENSOR_OK);

  int32_t v = 0;
  CHECK_EQ((int)sensor_read(&sa, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 2500);
  CHECK_EQ((int)sensor_read(&sa, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 2510);
  CHECK_EQ((int)sensor_read(&sb, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 1800); // not 2520, and not a drifted 1810
  CHECK_EQ((int)sensor_read(&sa, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 2520); // sa's ramp continued where SA left it
}

TEST("different backends live behind the same interface") {
  struct thermistor_ctx t = {.base_centi_c = 2500, .drift_per_read = 10};
  struct encoder_ctx e = {.position = 100, .detent_step = 4};
  struct sensor therm = {&thermistor_ops, &t};
  struct sensor knob = {&encoder_ops, &e};
  CHECK_EQ((int)sensor_start(&therm), (int)SENSOR_OK);
  CHECK_EQ((int)sensor_start(&knob), (int)SENSOR_OK);

  int32_t v = 0;
  CHECK_EQ((int)sensor_read(&therm, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 2500); // starting knob must not have hijacked therm
  CHECK_EQ((int)sensor_read(&knob, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 100);
  CHECK_EQ((int)sensor_read(&knob, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 104);
  CHECK_EQ((int)sensor_read(&therm, &v), (int)SENSOR_OK);
  CHECK_EQ(v, 2510);
}
