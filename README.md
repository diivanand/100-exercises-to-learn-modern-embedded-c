# 100 exercises to learn modern embedded C

> **Status (25 September 2026).** As the author works through this
> AI-assisted, generated repo, this section will be updated with how much
> has been vetted and gone through by the human author, so you know which
> parts have been human-vetted. **So far: none of it** — an exercise may
> contain mistakes in its prose, its starter or its tests, and they get
> fixed as each one is reached. Every solution passes its tests on the
> host toolchain and the whole tree is checked by CI; the NUCLEO chapters
> (15 to 17) additionally cross-compile for the Cortex-M4, but **have not
> yet been run on the board** — the first flash is still to come.

Learn modern C — C17, the C of shipping firmware — by fixing, finishing and
writing 100 small programs. Then, if you have a NUCLEO-L476RG within reach,
carry on into 15 more that run on the bare metal itself (chapters 15 to 17,
[below](#the-nucleo-track-chapters-15-to-17)).

Every exercise is a single self-contained file that states a problem and
checks your answer. You edit the code, run the tests, and move on when they
pass. There is no lecture to sit through and nothing to read that is not
next to the code it is about.

This is the embedded C sibling of
[100 exercises to learn modern C++](https://github.com/diivanand/100_exercises_to_learn_modern_cpp),
which is itself the C++ counterpart to
[100 exercises to learn Rust](https://github.com/mainmatter/100-exercises-to-learn-rust).

The course assumes you have read K&R (2nd edition) at some point and written
C before. It does not reteach syntax; it teaches the discipline — the
integer promotions, the memory maps, the volatile semantics, the test
seams — that separates C that works from C that ships.

## Why "embedded" C

Everything here runs on your Mac (or any Linux box). What makes it embedded
is the *discipline*, not the hardware:

- **The budget is an MCU's.** 128 KB of RAM, a 2 KB stack, no heap after
  chapter 08 shows you what to use instead. Chapter 06 tells you which
  section every byte lands in and who pays for it at boot.
- **The hardware is a parameter.** Register blocks are structs the tests
  inject (chapter 11), time is a tick the tests control (chapter 13), and
  the ISR is a thread standing in for the real one (chapter 12). This is
  Grenning's dual-targeting: logic that runs on the host meets sanitizers
  the target will never fit.
- **Nothing beyond C17**, and nothing your cross-compiler or an embedded
  Linux toolchain would refuse. No C23, no GNU extensions in exercise code.

## Requirements

- **A C17 toolchain.** Apple clang (via `xcode-select --install`) or any
  recent clang/GCC. CI builds macOS clang and Linux GCC.
- **CMake 3.24+** and **Ninja** — `brew install cmake ninja`.
- Optionally **clang-format** and **clang-tidy** — `brew install llvm`.

Nothing else. The test harness (mect, ~400 lines of plain C you are
encouraged to read) is vendored in `third_party/`, so the project configures
and builds with no network access. The NUCLEO chapters additionally need the
ARM toolchain and the board; without them they are skipped automatically and
the first 100 exercises are unaffected.

## Getting started

```sh
git clone <this repo>
cd 100-exercises-to-learn-modern-embedded-c
./mec next
```

`./mec next` builds and runs the first exercise that is not yet passing, and
shows you its failure. That is the whole loop:

1. Run `./mec next`.
2. Open the file it names.
3. Make the tests pass.
4. Repeat.

When every exercise passes, you are done.

## The commands

```
./mec next                 the main loop: run the first unfinished exercise
./mec test <filter>        run one exercise    (./mec test 03_06, or function_pointers)
./mec verify               run everything, as CI does
./mec list                 the curriculum, and where you are in it
./mec solution <filter>    diff your work against the reference solution
./mec build <filter>       build without running
./mec flash <filter>       build, flash and run one NUCLEO exercise on the board
./mec listen               watch the board's serial output
./mec format [--check]     clang-format the tree
./mec tidy [filter]        clang-tidy the tree
./mec clean                remove the build directories
```

A filter is any substring of an exercise id, so `07`, `07_06`, `x_macros`
and `06_x_macros` all select 07.06.

## How an exercise works

Each `exercises/<chapter>/<exercise>/exercise.c` has three parts:

- **A header comment** explaining the idea, why it exists, and where it
  bites. This is the teaching material; there is no separate book. Claims
  cite their sources — CERT C rules by id, Effective C by chapter, the
  STM32 reference manual by section — so you can check any of them.
- **The code you edit**, marked with `TODO`.
- **The tests**, which are the specification. Read them — several exercises
  have a test that exists specifically to catch a plausible shortcut.

Some exercises **start as a compile error**, because for some lessons the
compiler is the test. Those say so in a `NOTE` in their header comment.

A few exercises are **compiled with the optimiser on** (an `optimize` marker
file in their directory), because their bug — a missing `volatile` — is
invisible at `-O0`. Their headers say so; expect the starter to *hang*, and
the harness to tell you after thirty seconds.

## Sanitizers are part of the course

A buffer overrun that happens to produce the right answer is still a buffer
overrun. Where an exercise is about memory or concurrency, its header says
to re-run it under a sanitizer:

```sh
cmake --preset asan && ctest --preset asan       # memory + undefined behaviour
cmake --preset tsan && ctest --preset tsan       # data races (chapter 12)
```

These are host-only tools — an MCU cannot run them — which is precisely the
argument for keeping your firmware logic host-testable. Learning to reach
for them is part of the course.

## The curriculum

| # | Chapter | What it covers |
|---|---------|----------------|
| 00 | Getting started | the workflow, reading diagnostics, undefined behaviour and sanitizers |
| 01 | Objects and types | stdint types, `size_t`, initialization, designated initializers, compound literals, enums, `const` and flash, bool traps |
| 02 | Integers and bits | promotions, signed/unsigned, overflow, shifts, masks, endianness, bitfields, alignment, fixed-point |
| 03 | Pointers and arrays | out-params, decay, arithmetic, const placement, matrices, function pointers, `void *` + context, `restrict` |
| 04 | Strings and memory | bounded copying, `snprintf`, `strtol`, tokenizing without state, mem functions, string tables in flash |
| 05 | Structs and unions | padding, serialization, lawful type punning, tagged unions, opaque types, flexible array members |
| 06 | Storage and linkage | storage duration, internal linkage, extern, `.data`/`.bss`/`.rodata`, static locals, `static inline` |
| 07 | The preprocessor | macro hygiene, `do{}while(0)`, `#`/`##`, conditional compilation, `_Static_assert`, X-macros, `_Generic` |
| 08 | Memory without malloc | heap correctness, why MCUs ban it, object pools, arenas, ring buffers, stack discipline |
| 09 | Errors and APIs | status enums, out-params, `goto cleanup`, errno, assert vs error, defensive boundaries, init/deinit |
| 10 | Modules and test doubles | module pattern, HAL boundaries, spies, fake clocks, mocks, characterization tests, vtables |
| 11 | Volatile and MMIO | volatile semantics, register structs, read-modify-write, write-1-to-clear, timeouts, field macros, barriers |
| 12 | Interrupts and atomics | shared data, C11 stdatomic, critical sections, SPSC rings, memory order, reentrancy, deferred work |
| 13 | State machines and time | switch/table/function-pointer FSMs, tick wraparound, software timers, debounce, the superloop |
| 14 | Capstone | CRC-16, a framed wire protocol, a fixed-point filter, a command station — assembled from everything above |
| 15 | Bare-metal bring-up | linker script, startup code, SysTick, GPIO, UART, the button — on the NUCLEO-L476RG |
| 16 | Peripheral drivers | EXTI interrupts, a real scheduler, interrupt-driven UART, ADC, DMA |
| 17 | Target capstone | chapter 14's protocol on real wires, telemetry, and a watchdog soak test |

Chapters 15 to 17 need the board; see below.

## The NUCLEO track (chapters 15 to 17)

The last three chapters run on real hardware: an
[ST NUCLEO-L476RG](https://www.st.com/en/evaluation-tools/nucleo-l476rg.html)
(STM32L476RG: Cortex-M4F, 1 MB flash, 128 KB RAM), which plugs into USB and
carries its own ST-Link programmer — no other equipment needed.

They assume everything before them and use it on silicon: the register
structs of chapter 11 become the real GPIO block, chapter 12's SPSC ring
feeds a real interrupt handler, chapter 14's protocol arrives over a real
UART. The board support in `bsp/` was written the way the course teaches —
hand-made register definitions with `_Static_assert`ed offsets, a commented
linker script, startup code whose three jobs are exercise 15.02 — and
reading it is encouraged.

### Requirements on top of the host course

```sh
brew install --cask gcc-arm-embedded     # arm-none-eabi-gcc + newlib
brew install stlink                      # st-flash
```

(On Linux: `apt install gcc-arm-none-eabi stlink-tools`.)

### The workflow

```sh
./mec flash 15_01     # build, flash, and read the test results over serial
./mec listen          # just watch the serial port
```

`./mec flash` cross-compiles the exercise against `bsp/`, writes it to the
board over the ST-Link, then watches the virtual COM port for the harness's
summary line — the same tests, the same output, arriving at 115200 baud.
Passing exercises are remembered in `.mec-target-passed`, and `./mec next`
and `./mec list` treat them as done. A few exercises are interactive (they
ask you to press the blue button) and say so in their headers; chapter 17's
feed the board protocol frames from a `feed.txt` in the exercise directory.

Total silence after a flash usually means the image did not boot — which in
this track is a lesson, not an accident: 15.01 starts with a broken linker
script and 15.02 with empty startup code.

CI cross-compiles every NUCLEO exercise and solution with arm-none-eabi-gcc
on every push, which catches the large majority of mistakes; the board
catches the rest.

## Solutions

Every exercise has a reference solution in `solutions/`, with comments
explaining the choices rather than just the mechanics.

```sh
./mec solution 03_06    # diff your work against it
```

Try to reach for them only after you have a failing attempt of your own —
the diff teaches more when you have already made the decision it disagrees
with.

To build and test them all:

```sh
cmake --preset solutions && ctest --preset solutions
```

## The build

This is a normal modern CMake project, and it is worth reading:

- `CMakeLists.txt` — interface targets carrying options and warnings, no
  global state.
- `cmake/CompilerWarnings.cmake` — the warning set, C-flavoured (from
  cppbestpractices and Effective C ch. 11), `-Werror` on by default. The
  compiler is the cheapest static analyser you have.
- `cmake/Sanitizers.cmake` — ASan/UBSan and TSan wiring.
- `cmake/arm-none-eabi.cmake` — the cross toolchain for the NUCLEO track.
- `cmake/AddExercise.cmake` — exercise discovery: add a directory, get a
  target and a test. Chapters 15+ become `.elf`/`.bin` pairs instead.
- `CMakePresets.json` — the presets the commands above use.

If a warning is genuinely in your way, configure with
`-DMEC_WARNINGS_AS_ERRORS=OFF`. Use it to keep moving, not as a habit.

### A note on clang-tidy

`.clang-tidy` turns on `bugprone-*`, `cert-*`, `clang-analyzer-*`,
`concurrency-*`, `misc-*`, `performance-*`, `portability-*` and
`readability-*`, then switches off a short list of checks — each with the
reason written next to it in the file. Read that list before copying the
file into a project of your own; several checks are off only because
teaching code deliberately demonstrates what they exist to prevent.

## Using this with CLion

Open the directory. CLion reads `CMakePresets.json` and offers the presets
(`debug`, `solutions`, `asan`, `tsan`, `target`) directly — pick `debug`.

- **Run one exercise**: pick its `ex_<chapter>_<name>` target.
- **Run everything**: the `exercises` target, then the CTest configuration.
- `.clang-format` and `.clang-tidy` are picked up automatically; turn on
  *Settings → Editor → Code Style → Enable ClangFormat*.
- For the NUCLEO chapters, `./mec flash` from CLion's terminal is the
  simplest path; the `target` preset gives code completion against the
  cross-compiled configuration.

## The test harness

`third_party/mect/` is this course's harness: auto-registering `TEST()`
cases, a `_Generic`-dispatched `CHECK_EQ` (07.07 dissects it), one
`putc`-shaped port that prints to stdout on the host and USART2 on the
board. It is ~400 lines of the same C the course teaches, and it is the
answer to "what does a test framework cost on a microcontroller".

The industry equivalent is [Unity](https://www.throwtheswitch.org/unity),
which "Test-Driven Development for Embedded C" uses throughout; the shape
is the same, only the registration is manual there.

## Where this material comes from

- **Robert C. Seacord, *Effective C*, 2nd ed. (No Starch, 2024)** — the
  language backbone: chapters cited throughout as "Effective C ch. N".
- **The [CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c)** —
  cited by rule id (INT02-C, STR31-C, …) so you can read the source of any
  claim, exactly as the C++ course cites the Core Guidelines.
- **James W. Grenning, *Test-Driven Development for Embedded C*
  (Pragmatic Bookshelf, 2011)** — chapters 10 and the whole host-first
  architecture of the course; cited by chapter.
- **ST RM0351** (STM32L4 reference manual) and **UM1724** (Nucleo-64 user
  manual) — every register offset and pin in chapters 11 and 15–17, cited
  by section.
- **[100 exercises to learn Rust](https://github.com/mainmatter/100-exercises-to-learn-rust)** —
  the format, via its C++ sibling.

## License

The exercises and solutions are released under CC BY-NC 4.0, matching the
Rust course this is modelled on. See `LICENSE`.
