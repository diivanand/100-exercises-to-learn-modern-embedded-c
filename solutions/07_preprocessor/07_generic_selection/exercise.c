// Solution -- 07.07 _Generic: dispatch on type, at compile time

#include <mect/mect.h>

#include <stdint.h>

static int abs_i(int v) {
  return v < 0 ? -v : v; // INT_MIN would overflow; see the header note
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

// Each association maps a TYPE to a FUNCTION NAME; the (x) after the closing
// parenthesis then calls whichever one was selected. Only the selected
// branch is type-checked against the call, which is why one macro can serve
// four signatures.
#define abs_val(x)                                                                       \
  _Generic((x), int: abs_i, long long: abs_ll, float: abs_f, double: abs_d)(x)

// A _Generic that maps types to string literals -- no call, just selection.
// Note what is NOT here: no promotion happens, so float selects "float" and
// only an actual int selects "int".
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
