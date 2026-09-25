# Contributing

## Adding an exercise

1. Create `exercises/<NN_chapter>/<NN_name>/exercise.c` and
   `solutions/<NN_chapter>/<NN_name>/exercise.c`. CMake globs the tree, so
   there is nothing to register — reconfigure and the target exists.

2. Write the **solution first**. It is the specification, and writing it first
   is the only reliable way to find out whether the exercise is well posed.

3. Derive the starter from it: keep the tests verbatim, replace the
   implementation with a stub or a plausibly-wrong version, and mark it `TODO`.

4. Run `./scripts/check-course.sh`.

## What the checker enforces

- **Every solution builds and passes.** A solution that does not is worse than
  no solution.
- **Every host starter fails** — to compile or to pass. An exercise that
  already passes teaches nothing, and this is the most common way for the
  course to rot as the compiler changes underneath it.
- **A starter that does not compile says so** in a `NOTE` line in its header
  comment, so nobody is left staring at an error the course did not warn them
  about. The converse is checked too: a `NOTE` that promises a compile error
  fails the check if the starter in fact builds.
- **NUCLEO starters and solutions (chapters 15–17) cross-compile** when
  arm-none-eabi-gcc is on the PATH. They are exempt from "must fail" — a wrong
  register write cannot be observed without the board.

## The C rules

- **C17, no extensions.** Everything compiles with `-std=c17 -Wpedantic` and
  the full warning set in `cmake/CompilerWarnings.cmake`, as errors. Nothing
  from C23: no `nullptr`, no `constexpr`, no `typeof`, no `[[attributes]]`,
  no `#embed`, no `_BitInt`. If a C23 feature is genuinely the future answer,
  the header comment may *mention* it; the code may not use it.
  (One tolerated extension: `__attribute__((constructor))` inside the vendored
  harness. Exercise code itself stays standard.)
- **Nothing an MCU cannot do.** No `malloc` outside chapters 08/09 — and there
  only to make a point. No VLAs (`-Wvla` is an error). No recursion without a
  stated depth bound. No `float`/`double` where integers do (the exceptions:
  CHECK_NEAR tolerances in tests, and exercises explicitly about fixed-point
  versus float). Host-only facilities (POSIX threads, `errno` from libc calls)
  appear only in exercises that are explicitly about the host or embedded
  Linux, and say so.
- **Single translation unit.** An exercise is one `exercise.c`. Where the
  lesson is about headers and linkage (chapter 06), the file simulates the
  split with clearly-marked sections. NUCLEO exercises may additionally ship a
  `link.ld` (15.01 does) or a `feed.txt` of hex lines for `./mec flash` to
  send to the board (chapter 17).
- **A per-exercise `optimize` marker file** makes CMake compile that exercise
  with `-O2` — for the handful of lessons that are invisible at `-O0`, such as
  a missing `volatile`. The header comment must say the flag is in play.

## Style

- `.clang-format` is in the root and not up for debate; `./mec format` before
  committing.
- **snake_case** for functions, variables and types; **UPPER_CASE** for
  macros and enum constants. Struct tags over typedef soup; when a typedef
  earns its keep, no `_t` suffix (POSIX reserves it).
- Cite sources by name and number so claims can be checked: CERT C rules
  (INT30-C, STR31-C, …), Effective C 2nd ed. by chapter, TDD for Embedded C
  by chapter, the STM32L4 reference manual (RM0351) by section, MISRA C by
  rule where relevant.
- Exercise prose is British-flavoured plain English: short sentences, no
  exclamation marks, no "simply", no "obviously".

## What makes a good exercise here

- **The header comment is the teaching material.** There is no separate book,
  so the comment has to explain the idea, why it exists, and what goes wrong
  without it — including on the hardware this course cares about (a Cortex-M4
  with 128 KB of RAM, or a small embedded Linux box).

- **The bug should be one somebody would really write.** A starter that fails
  because a function returns `0` is acceptable only for "implement this";
  where the lesson is a trap, the starter should have *fallen into the trap* —
  the `~x == 0` that is never true, the `i <= size` that clears the canary,
  the `reg |= FLAG` on a write-1-to-clear register.

- **The tests are the specification.** Include at least one test that catches
  a plausible shortcut, and make the failure diagnosable: a wrong value with a
  printed comparison beats a segfault. Where the bug genuinely is memory
  corruption, say in the header to re-run under `cmake --preset asan`.

- **Failures must be deterministic.** A test that fails one run in fifty is
  worse than no test. Where the lesson is a race (chapter 12), either make the
  wrong version fail overwhelmingly (enough iterations that passing by luck is
  negligible) or test the intent — the declared atomicity, the balanced
  critical section — rather than the timing.

- **Respect the budget.** Tests finish in seconds; buffers are sized like an
  MCU's, not a server's. The harness registry holds 64 tests per exercise;
  nothing needs more than ten.

## NUCLEO exercises (chapters 15–17)

- Everything above applies, plus: registers are defined by hand from RM0351
  with `_Static_assert(offsetof(...))` guards, never copied wholesale from
  vendor headers. The board support in `bsp/` owns the vector table, startup
  and the UART the harness prints through; an exercise overrides its weak
  hooks when the lesson is startup itself.
- A test that needs a human (press the blue button) must say so in its header
  and give a generous deadline; `./mec flash` allows 60 seconds.
- Until they have been run on a board, on-target exercises are marked as
  unverified in the README status note. Do not remove that mark on faith.
