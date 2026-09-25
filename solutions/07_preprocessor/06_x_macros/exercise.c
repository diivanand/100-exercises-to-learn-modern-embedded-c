// Solution -- 07.06 X-macros: one list, every expansion

#include <mect/mect.h>

#include <stddef.h>

// THE list. Add a code here -- with its name -- and the enum, the name
// table and the count all update together. Nothing else to remember.
#define ERROR_LIST(X)                                                                    \
  X(ERR_OK, "ok")                                                                        \
  X(ERR_TIMEOUT, "timeout")                                                              \
  X(ERR_CRC, "crc mismatch")                                                             \
  X(ERR_BUSY, "busy")                                                                    \
  X(ERR_OVERRUN, "overrun")

#define X_AS_ENUM(sym, str) sym,
#define X_AS_NAME(sym, str) str,

enum error_code { ERROR_LIST(X_AS_ENUM) ERROR_CODE_COUNT };

static const char *const error_names[] = {ERROR_LIST(X_AS_NAME)};

// With both expansions generated from ERROR_LIST they cannot drift, but the
// assert documents the invariant for free (07.05).
_Static_assert(sizeof error_names / sizeof error_names[0] == ERROR_CODE_COUNT,
               "error_names is generated from the same list as the enum");

static size_t error_name_count(void) {
  return sizeof error_names / sizeof error_names[0];
}

static const char *error_name(enum error_code code) {
  if ((size_t)code >= error_name_count()) {
    return "unknown";
  }
  return error_names[code];
}

TEST("every code maps to its own name") {
  CHECK_EQ(error_name(ERR_OK), "ok");
  CHECK_EQ(error_name(ERR_CRC), "crc mismatch");
  CHECK_EQ(error_name(ERR_OVERRUN), "overrun");
}

TEST("the table and the enum agree on how many codes exist") {
  CHECK_EQ(error_name_count(), (size_t)ERROR_CODE_COUNT);
  CHECK_EQ((int)ERROR_CODE_COUNT, 5);
}

TEST("an out-of-range code degrades politely") {
  CHECK_EQ(error_name((enum error_code)99), "unknown");
}
