// Solution -- 13.03 The state IS a function pointer

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

enum motor_event {
  EV_GO,
  EV_STOP,
  EV_OVERCURRENT,
  EV_RESET,
};

struct motor;
struct state;

// A state is a handler plus an optional entry action plus a name for
// humans. Handlers RETURN the next state instead of assigning it -- the
// dispatcher owns the one place where state actually changes.
typedef const struct state *(*state_handler)(struct motor *m, enum motor_event ev);

struct state {
  state_handler handle;
  void (*on_entry)(struct motor *m);
  const char *name;
};

struct motor {
  const struct state *current;
  unsigned starts; // entry actions, counted so tests can see them
  unsigned stops;
  unsigned faults;
};

// Handlers and entry actions first as prototypes, then the three state
// objects, then the bodies (which refer to the objects). No forward
// "tentative" definitions needed -- just this ordering.
static const struct state *stopped_handle(struct motor *m, enum motor_event ev);
static const struct state *running_handle(struct motor *m, enum motor_event ev);
static const struct state *fault_handle(struct motor *m, enum motor_event ev);
static void enter_stopped(struct motor *m);
static void enter_running(struct motor *m);
static void enter_fault(struct motor *m);

static const struct state STATE_STOPPED = {stopped_handle, enter_stopped, "stopped"};
static const struct state STATE_RUNNING = {running_handle, enter_running, "running"};
static const struct state STATE_FAULT = {fault_handle, enter_fault, "fault"};

static void enter_stopped(struct motor *m) {
  ++m->stops; // in real firmware: gate drivers off, brake as configured
}

static void enter_running(struct motor *m) {
  ++m->starts; // in real firmware: precharge, enable PWM
}

static void enter_fault(struct motor *m) {
  ++m->faults; // in real firmware: latch the fault register, kill PWM
}

// Each handler answers one question -- "in THIS state, what does this
// event mean?" -- and answers it by returning a state. Returning
// m->current (via the state's own object) means "stay put".
static const struct state *stopped_handle(struct motor *m, enum motor_event ev) {
  (void)m;
  return (ev == EV_GO) ? &STATE_RUNNING : &STATE_STOPPED;
}

static const struct state *running_handle(struct motor *m, enum motor_event ev) {
  (void)m;
  switch (ev) {
  case EV_STOP:
    return &STATE_STOPPED;
  case EV_OVERCURRENT:
    return &STATE_FAULT;
  case EV_GO:
  case EV_RESET:
    return &STATE_RUNNING;
  }
  return &STATE_RUNNING;
}

static const struct state *fault_handle(struct motor *m, enum motor_event ev) {
  (void)m;
  // A latched fault clears only on an explicit reset. GO must not work --
  // that is the entire point of latching.
  return (ev == EV_RESET) ? &STATE_STOPPED : &STATE_FAULT;
}

void motor_init(struct motor *m) {
  *m = (struct motor){.current = &STATE_STOPPED};
  // Initial entry does not run: the counters count TRANSITIONS. (A design
  // that runs entry on init is also defensible; pick one and write it down.)
}

const char *motor_state_name(const struct motor *m) {
  return m->current->name;
}

void motor_handle(struct motor *m, enum motor_event ev) {
  const struct state *next = m->current->handle(m, ev);
  // Entry actions fire on CHANGES only. A handler returning its own state
  // means "nothing happened" -- re-running the entry action would restart
  // the motor on every ignored event.
  if (next != m->current) {
    m->current = next;
    if (next->on_entry != NULL) {
      next->on_entry(m);
    }
  }
}

TEST("go, stop, and the entry actions that prove it") {
  struct motor m;
  motor_init(&m);
  CHECK_EQ(motor_state_name(&m), "stopped");

  motor_handle(&m, EV_GO);
  CHECK_EQ(motor_state_name(&m), "running");
  CHECK_EQ(m.starts, 1u);

  motor_handle(&m, EV_STOP);
  CHECK_EQ(motor_state_name(&m), "stopped");
  CHECK_EQ(m.stops, 1u);
}

TEST("overcurrent latches a fault; only reset clears it") {
  struct motor m;
  motor_init(&m);
  motor_handle(&m, EV_GO);

  motor_handle(&m, EV_OVERCURRENT);
  CHECK_EQ(motor_state_name(&m), "fault");
  CHECK_EQ(m.faults, 1u);

  motor_handle(&m, EV_GO); // must NOT restart a faulted motor
  CHECK_EQ(motor_state_name(&m), "fault");
  CHECK_EQ(m.starts, 1u);

  motor_handle(&m, EV_RESET);
  CHECK_EQ(motor_state_name(&m), "stopped");
}

TEST("ignored events do not re-run entry actions") {
  struct motor m;
  motor_init(&m);
  motor_handle(&m, EV_GO);
  CHECK_EQ(m.starts, 1u);

  motor_handle(&m, EV_GO);    // already running: ignore
  motor_handle(&m, EV_RESET); // means nothing while running: ignore
  CHECK_EQ(m.starts, 1u);     // the motor was started ONCE
  CHECK_EQ(motor_state_name(&m), "running");
}

TEST("two motors share the state objects but not the state") {
  struct motor a, b;
  motor_init(&a);
  motor_init(&b);

  motor_handle(&a, EV_GO);
  motor_handle(&a, EV_OVERCURRENT);

  CHECK_EQ(motor_state_name(&a), "fault");
  CHECK_EQ(motor_state_name(&b), "stopped"); // b never moved
  CHECK_EQ(b.faults, 0u);
}
