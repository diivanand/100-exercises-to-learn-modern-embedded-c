// =============================================================================
//  07.06 -- X-macros: one list, every expansion
// =============================================================================
//
//  Somewhere in this code base -- in most code bases -- an enum and a table
//  describe the same things and are maintained BY HAND, in parallel. Here it
//  is an error enum and its name strings. Someone added ERR_OVERRUN to the
//  enum; someone (the same someone, an hour later) forgot the string --
//  actually worse, they dropped a DIFFERENT line while resolving a merge, so
//  every name from ERR_CRC on is shifted one place. The bounds check in
//  error_name() keeps it from reading off the end, and the tests catch the
//  drift. Nothing would have caught it in a code base that logs the name and
//  moves on: the log would just quietly say "busy" for a CRC error, and some
//  future person debugging a checksum problem would be reading lies.
//
//  The preprocessor fix has a venerable name: the X-MACRO. Write the list
//  ONCE, as calls to a macro X that is not yet defined; then define X to be
//  each thing you need, and expand the list through it:
//
//      #define ERROR_LIST(X) \
//        X(ERR_OK, "ok")     \
//        X(ERR_TIMEOUT, "timeout")
//
//      #define X_AS_ENUM(sym, str) sym,
//      #define X_AS_NAME(sym, str) str,
//
//      enum error_code { ERROR_LIST(X_AS_ENUM) ERROR_CODE_COUNT };
//      static const char *const error_names[] = { ERROR_LIST(X_AS_NAME) };
//
//  One source of truth; the enum, the strings and the count regenerate from
//  it on every compile. The same trick fans out to whatever else the list
//  needs -- a handler table, an init call per driver, a log level per
//  subsystem. Any time you find an enum with a parallel array (04.07's
//  string tables), this is the upgrade.
//
//  Costs, because there are some: the symbol ERR_CRC now appears in the
//  source only inside the list, so naive grep finds one hit where it used
//  to find the definition; and single-stepping through generated tables is
//  drearier than through written-out ones. Teams accept this for lists that
//  CHANGE; for a list frozen years ago, hand-written and asserted (07.05)
//  is honest too.
//
//  TASK
//    Replace the hand-maintained enum and name table with an X-macro list,
//    so they cannot drift again. Keep error_name()'s bounds check. Do not
//    change the tests -- ERROR_CODE_COUNT must come out of the expansion.
//
//  RUN IT
//    ./mec test 07_06
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>

// TODO: two parallel lists, drifted. Make them one list, expanded twice.
enum error_code { ERR_OK, ERR_TIMEOUT, ERR_CRC, ERR_BUSY, ERR_OVERRUN, ERROR_CODE_COUNT };

static const char *const error_names[] = {
    "ok",
    "timeout",
    "busy",
    "overrun",
};

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
