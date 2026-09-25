// =============================================================================
//  07.07 -- _Generic: dispatch on type, at compile time
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- "controlling expression
//  type ... not compatible with any generic association". Two of the tests
//  hand abs_val types its _Generic does not list yet.
//
//  C11's generic selection is an EXPRESSION that chooses between other
//  expressions by the TYPE of its first operand:
//
//      _Generic(x, int: abs_i, double: abs_d)(x)
//
//  At compile time, exactly one branch is selected (and only that branch
//  has to type-check); at run time nothing remains but the chosen call.
//  It is the closest thing C has to overloading, and the standard library
//  already uses it on your behalf: <tgmath.h>'s sin() picks sinf/sin/sinl
//  this way. (Effective C ch. 9, "Type-Generic Macros".)
//
//  You have been using it all course. Open third_party/mect/mect.h and read
//  CHECK_EQ: every integer type funnels to one signed or one unsigned
//  comparison function, floats to a double one, char* to strcmp, anything
//  else to the pointer branch via `default:`. Its controlling expression is
//  worth a minute of your time --
//
//      _Generic((1 ? (a) : (b)), ...)
//
//  The conditional operator applies the USUAL ARITHMETIC CONVERSIONS (02.01)
//  to its two arms, so mixing an int with an unsigned selects the unsigned
//  branch -- the same conversion == would perform. The macro does not dodge
//  C's conversion rules; it makes the result of them pick the printer.
//
//  The rules of the selection itself, which this exercise pins down:
//
//   - NO PROMOTIONS. A float selects `float:`, never `double:`. A uint8_t
//     variable selects `unsigned char:` -- not int, even though arithmetic
//     would promote it. (But beware: a character CONSTANT like 'a' really
//     IS an int in C, and _Generic will tell you so.)
//   - QUALIFIERS DROP. The controlling expression undergoes lvalue
//     conversion, so a `const uint32_t` selects plain `unsigned int:`.
//     (C11 as published got this wrong; C17's defect fixes settled it.)
//   - ARRAYS DECAY before selection -- a literal selects a pointer branch.
//   - `default:` catches everything unlisted; without it, an unlisted type
//     is a compile error. For a dispatch macro that is usually what you
//     want -- a LOUD error, not a silent wrong branch. abs_val below has no
//     default, on purpose, which is why the missing associations stop the
//     build instead of computing nonsense.
//
//  One honest caveat: `-v` on INT_MIN overflows (00.03), for abs_val(int)
//  exactly as for stdlib abs(). The fix (widen, or saturate) is 02.03's
//  business; the tests here stay away from the edge.
//
//  TASK
//    Give abs_val its two missing associations, routing each type to the
//    right function. Then read type_name and predict all its tests before
//    running them -- that is the real exercise.
//
//  RUN IT
//    ./mec test 07_07
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

static int abs_i(int v) {
  return v < 0 ? -v : v;
}
static long long abs_ll(long long v) {
  return v < 0 ? -v : v;
}
static float abs_f(float v) {
  return v < 0.0f ? -v : v;
}
static double abs_d(double v) {
  return v < 0.0 ? -v : v;
}

// TODO: two of the functions above are never selected, and the tests hand
// abs_val exactly those two types.
#define abs_val(x) _Generic((x), int: abs_i, double: abs_d)(x)

// Complete, for study: a _Generic that maps types to string literals -- no
// call, just selection.
#define type_name(x)                                                                     \
  _Generic((x),                                                                          \
      char: "char",                                                                      \
      signed char: "signed char",                                                        \
      unsigned char: "unsigned char",                                                    \
      int: "int",                                                                        \
      unsigned int: "unsigned int",                                                      \
      float: "float",                                                                    \
      double: "double",                                                                  \
      char *: "char *",                                                                  \
      const char *: "const char *",                                                      \
      default: "something else")

TEST("abs_val dispatches across all four types") {
  CHECK_EQ(abs_val(-8), 8);
  CHECK_EQ(abs_val(-40000000000LL), 40000000000LL);
  CHECK_EQ(abs_val(-2.5), 2.5);
  CHECK_EQ((double)abs_val(-2.5f), 2.5); // float in, float out
  CHECK_EQ(abs_val(8), 8);
}

TEST("selection is exact: no promotions, no surprises spared") {
  // A character CONSTANT has type int in C -- this is not a bug in
  // _Generic, it is a fact about C that _Generic makes visible.
  CHECK_EQ(type_name('a'), "int");
  CHECK_EQ(type_name((char)'a'), "char");
  CHECK_EQ(type_name(1u), "unsigned int");
  CHECK_EQ(type_name(2.5f), "float");
  CHECK_EQ(type_name(2.5), "double");
}

TEST("qualifiers are dropped, unlisted types hit default") {
  const uint32_t reg = 5; // lvalue conversion sheds the const
  CHECK_EQ(type_name(reg), "unsigned int");

  int n = 0;
  CHECK_EQ(type_name(&n), "something else"); // int* has no association

  // Standard C says a string literal is char[N] (const-ness by convention
  // only, enforced by "thou shalt not write" UB). This course compiles with
  // -Wwrite-strings, which makes literals const char[N] for real -- and
  // _Generic, always honest, reports what the flag did.
  CHECK_EQ(type_name("literal"), "const char *");
}
