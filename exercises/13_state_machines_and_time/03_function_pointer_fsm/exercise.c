// =============================================================================
//  13.03 -- The state IS a function pointer
// =============================================================================
//
//  Third rendering of a state machine, and the one that scales: each state
//  is a FUNCTION (plus, here, an entry action and a name, bundled in a
//  const struct). The machine's "current state" is a pointer to one of
//  these. Dispatch is a single indirect call:
//
//      const struct state *next = m->current->handle(m, ev);
//
//  This is the "dynamic interface" of Test-Driven Development for Embedded
//  C (ch. 11) applied to states: behaviour selected by a pointer swap, no
//  switch anywhere. What it buys over 13.01/13.02:
//
//   - EACH STATE IS A UNIT. One function, reviewable and testable alone;
//     a machine of thirty states stays thirty small functions instead of
//     one thousand-line switch.
//   - ENTRY ACTIONS HAVE A HOME. "On entering FAULT, kill the PWM and
//     latch the register" lives with FAULT, not scattered on every
//     transition that happens to arrive there.
//
//  Two disciplines make the pattern safe:
//
//   1. HANDLERS RETURN THE NEXT STATE; only the dispatcher assigns it.
//      A handler that mutates m->current directly works -- until entry
//      actions, tracing and two-instance reentrancy each need their own
//      copy of the bookkeeping. The single assignment point is where all
//      of it lives. (It is also why the same three const state objects
//      can serve any number of motors: per-instance data stays in struct
//      motor, the states hold none. The fourth test insists.)
//
//   2. ENTRY ACTIONS FIRE ON CHANGES ONLY. "Stay put" is a handler
//      returning its own state, and staying put must not restart the
//      motor. The dispatcher, not each handler, enforces this.
//
//  THE MACHINE: a motor controller.
//
//      STOPPED --GO--> RUNNING --STOP--> STOPPED
//      RUNNING --OVERCURRENT--> FAULT --RESET--> STOPPED
//
//  FAULT is LATCHED: GO does nothing there; only an explicit RESET leaves.
//  Entry actions count starts/stops/faults, standing in for the PWM-enable
//  and gate-driver work real firmware would do.
//
//  The starter has two bugs: RUNNING never leaves for FAULT (its author
//  left a TODO), and the dispatcher runs the entry action on EVERY event,
//  including ignored ones -- so an already-running motor is "restarted"
//  each time someone leans on the GO button.
//
//  TASK
//    Fix running_handle and motor_handle. Do not change the tests.
//
//  RUN IT
//    ./mec test 13_03
//
// =============================================================================

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

typedef const struct state *(*state_handler)(struct motor *m,
                                             enum motor_event ev);

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

static const struct state *stopped_handle(struct motor *m, enum motor_event ev);
static const struct state *running_handle(struct motor *m, enum motor_event ev);
static const struct state *fault_handle(struct motor *m, enum motor_event ev);
static void enter_stopped(struct motor *m);
static void enter_running(struct motor *m);
static void enter_fault(struct motor *m);

static const struct state STATE_STOPPED = {stopped_handle, enter_stopped,
                                           "stopped"};
static const struct state STATE_RUNNING = {running_handle, enter_running,
                                           "running"};
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

static const struct state *stopped_handle(struct motor *m,
                                          enum motor_event ev) {
  (void)m;
  return (ev == EV_GO) ? &STATE_RUNNING : &STATE_STOPPED;
}

static const struct state *running_handle(struct motor *m,
                                          enum motor_event ev) {
  (void)m;
  switch (ev) {
  case EV_STOP:
    return &STATE_STOPPED;
  case EV_OVERCURRENT:
    // TODO: "handle overcurrent properly later" -- and later never came.
    // An overcurrent that keeps the motor running is a fire waiting for
    // its paperwork.
    return &STATE_RUNNING;
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
}

const char *motor_state_name(const struct motor *m) {
  return m->current->name;
}

void motor_handle(struct motor *m, enum motor_event ev) {
  const struct state *next = m->current->handle(m, ev);
  // TODO: this runs the entry action on EVERY event, whether or not the
  // state changed. Lean on GO while running and watch `starts` climb.
  m->current = next;
  if (next->on_entry != NULL) {
    next->on_entry(m);
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
