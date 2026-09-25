// =============================================================================
//  13.04 -- Tick arithmetic that survives the wrap
// =============================================================================
//
//  Firmware time is a uint32_t counting milliseconds. It wraps at
//  4,294,967,296 ms -- every 49.7 days. This is firmware's private Y2K,
//  except it recurs, and it only bites products that ship: nothing wraps
//  on the bench, everything wraps in the field. The 497-day family of
//  uptime bugs (49.7 days x a 10 ms tick) has hit flight software, network
//  gear and at least one power plant's monitoring -- the write-up is
//  always the same: "worked for weeks, then every timer fired at once" or
//  "then no timer ever fired again".
//
//  The rules, and they are short:
//
//   1. NEVER STORE OR COMPARE ABSOLUTE DEADLINES. `if (now >= deadline)`
//      breaks the day `deadline = start + duration` wraps past zero and
//      becomes numerically tiny: the comparison is true 49 days early.
//
//   2. ALWAYS COMPARE ELAPSED AGAINST DURATION:
//
//          (uint32_t)(now - start) >= duration
//
//      Unsigned subtraction is modular (02.03 -- the wraparound that was
//      the trap there is the TOOL here), so `now - start` is the true
//      elapsed count even when `now` has wrapped and is numerically
//      smaller than `start`. Correct for durations up to 2^31 - 1.
//
//   3. "IS a AFTER b" is the modular difference landing in the first half
//      of the range: `(a - b) < 0x80000000`. The Linux kernel spells the
//      same test `(int32_t)(b - a) < 0` (jiffies.h); that cast is
//      implementation-defined (not undefined -- Effective C ch. 3), and
//      kernels get to assume twos-complement compilers. Portable code
//      keeps the comparison unsigned.
//
//  CERT INT30-C is about UNINTENDED unsigned wrap; this exercise is the
//  flip side it explicitly allows -- wrap on purpose, documented.
//
//  The starter commits both classic sins: an absolute-deadline comparison,
//  and a "protect against negative" clamp in ticks_since (there is no
//  negative; the clamp converts correct modular arithmetic into an answer
//  of 0 for seven weeks). The tests cross the wrap explicitly.
//
//  TASK
//    Fix all three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 13_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

uint32_t ticks_since(uint32_t now, uint32_t start) {
  // TODO: this clamp "protects" against a case that does not exist and
  // reports 0 elapsed for ~49 days after every wrap.
  return now > start ? now - start : 0;
}

bool deadline_reached(uint32_t now, uint32_t start, uint32_t duration) {
  // TODO: absolute deadline. `start + duration` wraps; `now` has not yet.
  return now >= start + duration;
}

bool time_after(uint32_t a, uint32_t b) {
  // TODO: plain > is not an ordering on a circle.
  return a > b;
}

TEST("ordinary elapsed time, no wrap in sight") {
  CHECK_EQ(ticks_since(1500, 1000), 500u);
  CHECK(deadline_reached(1500, 1000, 500));
  CHECK_FALSE(deadline_reached(1500, 1000, 501));
  CHECK(time_after(1500, 1000));
  CHECK_FALSE(time_after(1000, 1500));
  CHECK_FALSE(time_after(1000, 1000));
}

TEST("elapsed time straddling the 32-bit wrap") {
  // start 16 ticks before the wrap, now 16 ticks after it: 32 elapsed.
  CHECK_EQ(ticks_since(0x00000010u, 0xFFFFFFF0u), 0x20u);
  CHECK(deadline_reached(0x00000010u, 0xFFFFFFF0u, 0x20u));
  CHECK_FALSE(deadline_reached(0x00000010u, 0xFFFFFFF0u, 0x21u));
  CHECK(time_after(0x00000010u, 0xFFFFFFF0u));
  CHECK_FALSE(time_after(0xFFFFFFF0u, 0x00000010u));
}

TEST("a deadline that wraps must not fire early") {
  // start = 0xFFFFFF00, duration = 0x200: the absolute deadline would be
  // 0x100 -- numerically TINY. At now = 0xFFFFFF80 only 0x80 ticks have
  // elapsed; a timer comparing against the absolute value fires 384 ticks
  // early, which in a watchdog feeder or a motor commutation loop is not
  // a rounding error but an incident report.
  CHECK_FALSE(deadline_reached(0xFFFFFF80u, 0xFFFFFF00u, 0x200u));
  // ...and once 0x200 ticks HAVE elapsed (now = 0x100, wrapped), it fires.
  CHECK(deadline_reached(0x00000100u, 0xFFFFFF00u, 0x200u));
}

TEST("uptime keeps flowing after a wrap") {
  // The 49.7-day mark: one tick before the wrap, at it, after it.
  CHECK_EQ(ticks_since(0xFFFFFFFFu, 0xFFFFFFFEu), 1u);
  CHECK_EQ(ticks_since(0x00000000u, 0xFFFFFFFFu), 1u);
  CHECK_EQ(ticks_since(0x00000005u, 0xFFFFFFFBu), 10u);
}
