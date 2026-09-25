// =============================================================================
//  10.07 -- The vtable pattern: many devices, one interface
// =============================================================================
//
//  Grenning calls it the dynamic interface (ch. 11); C++ programmers will
//  recognise a hand-rolled virtual table. One struct of function pointers
//  per device TYPE, one context pointer per device INSTANCE:
//
//      struct sensor_ops {                       // per type, in .rodata
//        enum sensor_status (*start)(void *ctx);
//        enum sensor_status (*read)(void *ctx, int32_t *out);
//      };
//      struct sensor {                           // per instance
//        const struct sensor_ops *ops;
//        void *ctx;
//      };
//
//  Application code calls sensor_read(&s, &value) and neither knows nor
//  cares whether a thermistor or an encoder answers. New device? New ops
//  table and context struct; zero edits to callers -- the open/closed
//  principle in plain C.
//
//  The whole pattern stands on one discipline, and the starter breaks it
//  twice, both times the same way: SOMEBODY ADDED A SINGLETON.
//
//   1. The generic layer keeps a static `active_sensor`, set by start and
//      used -- instead of the argument -- by read. Start two sensors and
//      every read goes to whichever started LAST.
//   2. The thermistor backend copies its context into a file-static at
//      start ("there is only one thermistor on this board") and reads from
//      the static ever after. Two instances now share one state.
//
//  Each bug is invisible with one sensor on the bench and certain with
//  two, which is why both key tests run PAIRS. State that rides in the
//  context can multiply; state hiding in a static cannot -- the same
//  lesson 06.05 taught, now at interface scale.
//
//  Costs, for balance: an indirect call the optimiser cannot inline, and
//  ops tables to keep honest. For three known backends on a small MCU, a
//  plain switch is often the better trade; the vtable earns its keep when
//  the set of devices is open or when tests must slide a double behind the
//  same interface (10.02).
//
//  TASK
//    Remove both singletons: dispatch through the argument, keep all
//    backend state in the context. Do not change the tests.
//
//  RUN IT
//    ./mec test 10_07
//
// =============================================================================

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

// TODO: this "current sensor" is the first singleton. Dispatch must go
// through the argument, not through whoever started last.
static struct sensor *active_sensor;

static enum sensor_status sensor_start(struct sensor *s) {
  active_sensor = s;
  return s->ops->start(s->ctx);
}

static enum sensor_status sensor_read(struct sensor *s, int32_t *out) {
  (void)s; // "we know which sensor is active" -- do we?
  if (active_sensor == NULL) {
    return SENSOR_ERR_NOT_STARTED;
  }
  return active_sensor->ops->read(active_sensor->ctx, out);
}

// --- backend: thermistor (temperature ramps as the part self-heats) -------------

struct thermistor_ctx {
  int32_t base_centi_c;
  int32_t drift_per_read;
  int32_t reads;
  bool started;
};

// TODO: the second singleton. "There is only one thermistor on this board"
// -- until the second board spin, or the second test.
static struct thermistor_ctx the_thermistor;

static enum sensor_status thermistor_start(void *ctx) {
  the_thermistor = *(struct thermistor_ctx *)ctx;
  the_thermistor.reads = 0;
  the_thermistor.started = true;
  return SENSOR_OK;
}

static enum sensor_status thermistor_read(void *ctx, int32_t *out) {
  (void)ctx;
  struct thermistor_ctx *t = &the_thermistor;
  if (!t->started) {
    return SENSOR_ERR_NOT_STARTED;
  }
  *out = t->base_centi_c + t->drift_per_read * t->reads;
  ++t->reads;
  return SENSOR_OK;
}

static const struct sensor_ops thermistor_ops = {thermistor_start,
                                                 thermistor_read};

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
