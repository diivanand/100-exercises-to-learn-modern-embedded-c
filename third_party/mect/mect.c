// mect -- implementation. See mect.h for the API and the design notes.
//
// Notice what is NOT here: no malloc (the registry is a fixed-size static
// array -- chapter 08 explains why that is the embedded default), no threads,
// no globals a test could corrupt without the summary noticing. All output
// funnels through mect_print(), which funnels through the one port function.

#include "mect.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// --- output ------------------------------------------------------------------

static void mect_print(const char *fmt, ...) {
  // Format into a fixed buffer, then hand the bytes to the port one at a
  // time. 256 bytes is enough for any line the harness prints; vsnprintf
  // truncates rather than overruns if that ever stops being true (04.02).
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  const int n = vsnprintf(buffer, sizeof buffer, fmt, args);
  va_end(args);
  if (n < 0) {
    return;
  }
  for (const char *p = buffer; *p != '\0'; ++p) {
    if (*p == '\n') {
      mect_port_putc('\r'); // serial terminals want CRLF; stdout ignores \r
    }
    mect_port_putc(*p);
  }
}

// --- registry ----------------------------------------------------------------

struct mect_test {
  const char *name;
  mect_test_fn fn;
  const char *file;
  int line;
};

enum { MECT_MAX_TESTS = 64 };

static struct mect_test mect_tests[MECT_MAX_TESTS];
static int mect_test_count = 0;
static int mect_registry_overflow = 0;

void mect_register(const char *name, mect_test_fn fn, const char *file,
                   int line) {
  if (mect_test_count >= MECT_MAX_TESTS) {
    mect_registry_overflow = 1;
    return;
  }
  mect_tests[mect_test_count] =
      (struct mect_test){.name = name, .fn = fn, .file = file, .line = line};
  ++mect_test_count;
}

// --- per-test state ----------------------------------------------------------

static jmp_buf mect_escape;            // where REQUIRE/FAIL longjmp to
static int mect_checks_run = 0;        // across the whole run
static int mect_current_failed = 0;    // within the running test
static const char *mect_current_name;  // test being run, for failure headers

static const char *mect_basename(const char *path) {
  const char *base = path;
  for (const char *p = path; *p != '\0'; ++p) {
    if (*p == '/') {
      base = p + 1;
    }
  }
  return base;
}

void mect_report(bool ok, const char *kind, const char *expr, const char *file,
                 int line) {
  ++mect_checks_run;
  if (ok) {
    return;
  }
  if (!mect_current_failed) {
    // First failure in this test: name it, then list what went wrong.
    mect_print("[FAIL] %s\n", mect_current_name);
  }
  mect_current_failed = 1;
  mect_print("       %s:%d: %s( %s )\n", mect_basename(file), line, kind,
             expr);
}

void mect_fatal(void) {
  longjmp(mect_escape, 1);
}

// --- typed equality ----------------------------------------------------------

void mect_eq_ll(long long a, long long b, const char *expr, const char *file,
                int line) {
  mect_report(a == b, "CHECK_EQ", expr, file, line);
  if (a != b) {
    mect_print("         left:  %lld (0x%llx)\n", a, (unsigned long long)a);
    mect_print("         right: %lld (0x%llx)\n", b, (unsigned long long)b);
  }
}

void mect_eq_ull(unsigned long long a, unsigned long long b, const char *expr,
                 const char *file, int line) {
  mect_report(a == b, "CHECK_EQ", expr, file, line);
  if (a != b) {
    mect_print("         left:  %llu (0x%llx)\n", a, a);
    mect_print("         right: %llu (0x%llx)\n", b, b);
  }
}

void mect_eq_dbl(double a, double b, const char *expr, const char *file,
                 int line) {
  // Exact comparison, on purpose: use CHECK_NEAR when you mean "close".
  mect_report(a == b, "CHECK_EQ", expr, file, line);
  if (a != b) {
    mect_print("         left:  %.17g\n", a);
    mect_print("         right: %.17g\n", b);
  }
}

void mect_eq_str(const char *a, const char *b, const char *expr,
                 const char *file, int line) {
  const bool ok = (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
  mect_report(ok, "CHECK_EQ", expr, file, line);
  if (!ok) {
    mect_print("         left:  %s%s%s\n", a ? "\"" : "", a ? a : "NULL",
               a ? "\"" : "");
    mect_print("         right: %s%s%s\n", b ? "\"" : "", b ? b : "NULL",
               b ? "\"" : "");
  }
}

void mect_eq_ptr(const void *a, const void *b, const char *expr,
                 const char *file, int line) {
  mect_report(a == b, "CHECK_EQ", expr, file, line);
  if (a != b) {
    mect_print("         left:  %p\n", a);
    mect_print("         right: %p\n", b);
  }
}

void mect_near(double a, double b, double eps, const char *expr,
               const char *file, int line) {
  const double diff = (a > b) ? (a - b) : (b - a);
  mect_report(diff <= eps, "CHECK_NEAR", expr, file, line);
  if (diff > eps) {
    mect_print("         left:  %.17g\n", a);
    mect_print("         right: %.17g  (|diff| = %g)\n", b, diff);
  }
}

void mect_mem_eq(const void *a, const void *b, size_t n, const char *expr,
                 const char *file, int line) {
  const bool ok = memcmp(a, b, n) == 0;
  mect_report(ok, "CHECK_MEM_EQ", expr, file, line);
  if (!ok) {
    const unsigned char *pa = a;
    const unsigned char *pb = b;
    for (size_t i = 0; i < n; ++i) {
      if (pa[i] != pb[i]) {
        mect_print("         first difference at byte %zu: 0x%02x vs 0x%02x\n",
                   i, pa[i], pb[i]);
        break;
      }
    }
  }
}

// --- runner ------------------------------------------------------------------

int mect_run_all(void) {
  if (mect_registry_overflow) {
    mect_print("[mect] too many tests for the registry (max %d)\n",
               MECT_MAX_TESTS);
    return 1;
  }
  if (mect_test_count == 0) {
    // On the host this means a file with no TEST() -- a broken exercise. On
    // the target it usually means startup never walked init_array, so the
    // registrations never ran. 15.02 is about exactly this.
    mect_print("[mect] no tests were registered\n");
    return 1;
  }

  int failed_tests = 0;
  for (int i = 0; i < mect_test_count; ++i) {
    mect_current_failed = 0;
    mect_current_name = mect_tests[i].name;
    if (setjmp(mect_escape) == 0) {
      mect_tests[i].fn();
    }
    // (a longjmp from REQUIRE/FAIL lands here with the failure recorded)
    if (mect_current_failed) {
      ++failed_tests;
    } else {
      mect_print("[ ok ] %s\n", mect_tests[i].name);
    }
  }

  mect_print("[mect] %d test%s | %d passed | %d failed | %d check%s\n",
             mect_test_count, mect_test_count == 1 ? "" : "s",
             mect_test_count - failed_tests, failed_tests, mect_checks_run,
             mect_checks_run == 1 ? "" : "s");
  return failed_tests;
}
