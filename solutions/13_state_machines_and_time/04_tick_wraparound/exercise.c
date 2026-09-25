// Solution -- 13.04 Tick arithmetic that survives the wrap

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

uint32_t ticks_since(uint32_t now, uint32_t start) {
  // Unsigned subtraction is modular by definition (02.03), so this is
  // correct even when `now` has wrapped past zero and is numerically
  // SMALLER than `start`. No branch, no special case: the modular
  // arithmetic IS the special case, handled by the hardware for free.
  return now - start;
}

bool deadline_reached(uint32_t now, uint32_t start, uint32_t duration) {
  // Compare ELAPSED against DURATION -- never `now` against an absolute
  // deadline, because `start + duration` may wrap and absolute comparisons
  // do not. Correct for any duration up to 2^31 - 1 ticks.
  return ticks_since(now, start) >= duration;
}

bool time_after(uint32_t a, uint32_t b) {
  // "a is after b" when the modular difference a - b lands in (0, 2^31):
  // less than half the counter's range ahead. The Linux kernel spells this
  // `(int32_t)(b - a) < 0` (time_after in jiffies.h); the signed cast is
  // implementation-defined rather than undefined, and every twos-complement
  // compiler does what the kernel expects. The form below says the same
  // thing in fully portable arithmetic.
  return a != b && (a - b) < 0x80000000u;
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
