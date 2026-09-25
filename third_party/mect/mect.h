// mect -- a minimal C test harness for this course.
//
// This is deliberately small enough to read in one sitting, and reading it is
// encouraged: it is plain C17 and it uses several of the techniques the course
// teaches. TEST() registration is a function-pointer table (chapter 03),
// CHECK_EQ dispatches on the type of its arguments with _Generic (07.07
// explains how), REQUIRE unwinds with setjmp/longjmp, and the whole thing
// prints through one small seam -- mect_port_putc() -- so the same harness
// runs on your Mac (stdout) and on the NUCLEO board (UART).
//
// The API, which every exercise uses:
//
//   TEST(name) { ... }        define a test case; it registers itself
//   CHECK(cond)               non-fatal: record and continue
//   CHECK_FALSE(cond)
//   CHECK_EQ(a, b)            integers, floats, pointers, or C strings
//   CHECK_NEAR(a, b, eps)     floating point / fixed point tolerance
//   CHECK_MEM_EQ(p, q, n)     n bytes must match
//   REQUIRE(cond)             fatal: abandon this test case on failure
//   FAIL(msg)                 unconditional failure with a message
//
// mect is written for this course, but the real-world equivalent is Unity
// (ThrowTheSwitch), the harness used in "Test-Driven Development for Embedded
// C". The shape is the same; only the registration is manual there.

#ifndef MECT_MECT_H
#define MECT_MECT_H

#include <stdbool.h>
#include <stddef.h>

// --- registration ------------------------------------------------------------

typedef void (*mect_test_fn)(void);

void mect_register(const char *name, mect_test_fn fn, const char *file,
                   int line);

// Each TEST() expands to a function definition plus a tiny "constructor"
// function that the runtime calls before main(), which registers the test.
// __attribute__((constructor)) is a GCC/Clang extension, not standard C17 --
// the one extension this course permits itself, because the alternative is
// listing every test by hand in every file (which is exactly what Unity does,
// and exactly what people forget to do). On the bare-metal target there is no
// runtime to call constructors, so the startup code walks the init_array
// section itself -- exercise 15.02 makes you write that walk.
#define MECT_CONCAT2(a, b) a##b
#define MECT_CONCAT(a, b) MECT_CONCAT2(a, b)

#define TEST(test_name)                                                        \
  static void MECT_CONCAT(mect_test_, __LINE__)(void);                        \
  __attribute__((constructor)) static void MECT_CONCAT(mect_register_,        \
                                                       __LINE__)(void) {      \
    mect_register(test_name, MECT_CONCAT(mect_test_, __LINE__), __FILE__,     \
                  __LINE__);                                                  \
  }                                                                           \
  static void MECT_CONCAT(mect_test_, __LINE__)(void)

// --- checks ------------------------------------------------------------------

// All checks funnel into these reporting functions; the macros exist to
// capture the expression text, file and line. `mect_fatal` longjmps out of
// the current test case, so a failed REQUIRE never runs the code after it.
void mect_report(bool ok, const char *kind, const char *expr, const char *file,
                 int line);
void mect_fatal(void);

#define CHECK(cond) mect_report((cond), "CHECK", #cond, __FILE__, __LINE__)

#define CHECK_FALSE(cond)                                                      \
  mect_report(!(cond), "CHECK_FALSE", #cond, __FILE__, __LINE__)

#define REQUIRE(cond)                                                          \
  do {                                                                         \
    if (!(cond)) {                                                             \
      mect_report(false, "REQUIRE", #cond, __FILE__, __LINE__);                \
      mect_fatal();                                                            \
    }                                                                          \
  } while (0)

#define FAIL(msg)                                                              \
  do {                                                                         \
    mect_report(false, "FAIL", (msg), __FILE__, __LINE__);                     \
    mect_fatal();                                                              \
  } while (0)

// CHECK_EQ picks a comparison function from the type of its arguments, using
// C11's _Generic. The controlling expression `1 ? (a) : (b)` is a trick: the
// conditional operator applies the usual arithmetic conversions, so comparing
// an int with an unsigned selects the unsigned branch -- the same conversion
// the == operator would perform, made visible. 07.07 dissects this macro.
//
// Every integer type funnels into one signed or unsigned function; the
// argument conversion at the call does the widening. Comparing anything else
// (a struct, say) fails to compile, which is the correct answer.
#define CHECK_EQ(a, b)                                                         \
  _Generic((1 ? (a) : (b)),                                                    \
      _Bool: mect_eq_ll,                                                       \
      char: mect_eq_ll,                                                        \
      signed char: mect_eq_ll,                                                 \
      short: mect_eq_ll,                                                       \
      int: mect_eq_ll,                                                         \
      long: mect_eq_ll,                                                        \
      long long: mect_eq_ll,                                                   \
      unsigned char: mect_eq_ull,                                              \
      unsigned short: mect_eq_ull,                                             \
      unsigned int: mect_eq_ull,                                               \
      unsigned long: mect_eq_ull,                                              \
      unsigned long long: mect_eq_ull,                                         \
      float: mect_eq_dbl,                                                      \
      double: mect_eq_dbl,                                                     \
      char *: mect_eq_str,                                                     \
      const char *: mect_eq_str,                                               \
      default: mect_eq_ptr)((a), (b), #a " == " #b, __FILE__, __LINE__)

void mect_eq_ll(long long a, long long b, const char *expr, const char *file,
                int line);
void mect_eq_ull(unsigned long long a, unsigned long long b, const char *expr,
                 const char *file, int line);
void mect_eq_dbl(double a, double b, const char *expr, const char *file,
                 int line);
void mect_eq_str(const char *a, const char *b, const char *expr,
                 const char *file, int line);
void mect_eq_ptr(const void *a, const void *b, const char *expr,
                 const char *file, int line);

void mect_near(double a, double b, double eps, const char *expr,
               const char *file, int line);
#define CHECK_NEAR(a, b, eps)                                                  \
  mect_near((double)(a), (double)(b), (double)(eps),                           \
            #a " within " #eps " of " #b, __FILE__, __LINE__)

void mect_mem_eq(const void *a, const void *b, size_t n, const char *expr,
                 const char *file, int line);
#define CHECK_MEM_EQ(a, b, n)                                                  \
  mect_mem_eq((a), (b), (n), #a " == " #b " (" #n " bytes)", __FILE__,         \
              __LINE__)

// --- runner ------------------------------------------------------------------

// Runs every registered test and prints a summary. Returns the number of
// failed tests (0 registered tests counts as a failure: it means the
// registration machinery itself is broken, which on the bare-metal target is
// a real bug you will meet in 15.02).
int mect_run_all(void);

// The one place mect touches the outside world. The host implementation
// wraps putchar(); the NUCLEO implementation pushes the byte out USART2.
void mect_port_putc(char c);

#endif // MECT_MECT_H
