// Solution -- 08.02 Ownership is a convention, so write it down

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct report {
  uint32_t id;
};

// Live-object bookkeeping, and a free seam the tests can observe through.
// (int, not size_t: a double-destroy must show up as -1, not wrap.)
static int reports_live = 0;
static void (*report_free_fn)(void *) = free;

struct report *report_create(uint32_t id) {
  struct report *r = malloc(sizeof *r);
  if (r == NULL) {
    return NULL;
  }
  r->id = id;
  ++reports_live;
  return r;
}

void report_destroy(struct report *r) {
  if (r == NULL) {
    return;
  }
  --reports_live;
  report_free_fn(r);
}

// --- the queue ---------------------------------------------------------------
// report_submit TAKES OWNERSHIP on success: the queue destroys the report
// when it is flushed. On refusal (queue full) ownership STAYS WITH THE
// CALLER. That sentence is the API; everything below merely implements it.

enum { QUEUE_CAPACITY = 2 };
static struct report *queue[QUEUE_CAPACITY];
static size_t queue_len = 0;

bool report_submit(struct report *r) {
  if (queue_len == QUEUE_CAPACITY) {
    return false; // refused: still yours
  }
  queue[queue_len] = r; // accepted: now ours
  ++queue_len;
  return true;
}

void queue_flush(void) {
  for (size_t i = 0; i < queue_len; ++i) {
    report_destroy(queue[i]);
  }
  queue_len = 0;
}

// --- the callers under repair -------------------------------------------------

void send_or_drop(struct report *r) {
  // Destroy ONLY when the queue refused: on success the report is no
  // longer ours to touch, let alone destroy.
  if (!report_submit(r)) {
    report_destroy(r);
  }
}

size_t broadcast(const uint32_t *ids, size_t n) {
  size_t queued = 0;
  for (size_t i = 0; i < n; ++i) {
    struct report *r = report_create(ids[i]);
    if (r == NULL) {
      continue;
    }
    if (report_submit(r)) {
      ++queued;
    } else {
      report_destroy(r); // refused: ownership came back, and with it the duty
    }
  }
  return queued;
}

// --- test scaffolding: a recording free ----------------------------------------
// The recorder does NOT free, so even a buggy double-destroy stays free of
// undefined behaviour while the test measures it. The drain then frees each
// unique pointer exactly once.

enum { FREED_LOG_MAX = 16 };
static void *freed_log[FREED_LOG_MAX];
static size_t freed_len = 0;

static void recording_free(void *p) {
  if (freed_len < FREED_LOG_MAX) {
    freed_log[freed_len] = p;
  }
  ++freed_len;
}

static void begin_recording(void) {
  report_free_fn = recording_free;
  freed_len = 0;
}

static void drain_recorded(void) {
  report_free_fn = free;
  for (size_t i = 0; i < freed_len && i < FREED_LOG_MAX; ++i) {
    bool already_freed = false;
    for (size_t j = 0; j < i; ++j) {
      already_freed = already_freed || (freed_log[j] == freed_log[i]);
    }
    if (!already_freed) {
      free(freed_log[i]);
    }
  }
}

TEST("submit takes ownership; the caller must not destroy as well") {
  begin_recording();
  const int live_before = reports_live;
  struct report *r = report_create(7);
  REQUIRE(r != NULL);
  send_or_drop(r); // the queue has room, so ownership moves
  queue_flush();   // the queue destroys what it owns
  CHECK_EQ(reports_live - live_before, 0);
  CHECK_EQ(freed_len, 1u); // one report, one destroy -- exactly
  drain_recorded();
}

TEST("a refused submit hands ownership back to the caller") {
  begin_recording();
  const int live_before = reports_live;
  const uint32_t ids[] = {1, 2, 3};
  size_t queued = broadcast(ids, 3); // capacity is 2: the third is refused
  CHECK_EQ(queued, 2u);
  queue_flush();
  CHECK_EQ(reports_live - live_before, 0); // a leaked refusal is +1 here
  drain_recorded();
}
