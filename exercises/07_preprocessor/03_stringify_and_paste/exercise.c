// =============================================================================
//  07.03 -- Stringify, paste, and the indirection they both need
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR (an undeclared identifier
//  named `adc_chCURRENT_CHANNEL_raw` -- read on for why), and once that is
//  fixed the version-string tests still fail. Two symptoms, one cause.
//
//  Two operators exist only inside macro definitions (Effective C ch. 9):
//
//      #x     STRINGIFY: turn the argument's tokens into a string literal
//      a##b   PASTE: glue two tokens into one new token
//
//  They are how C builds compile-time tables of names (07.06 leans on
//  both), version strings, and register accessors. And they share one
//  famous gotcha: BOTH APPLY BEFORE THE ARGUMENT IS EXPANDED.
//
//      #define FW_MAJOR 3
//      #define STR(x) #x
//      STR(FW_MAJOR)              // "FW_MAJOR"  -- the NAME, not the value
//
//  The preprocessor's rule is: parameters touched by # or ## are used
//  as-written; everything else is macro-expanded first. So the fix is a
//  ladder of two macros. The outer one does nothing but FORWARD its
//  argument -- and forwarding is not touched by # or ##, so expansion
//  happens on the way through. The inner one then stringifies the result:
//
//      #define STR_(x) #x
//      #define STR(x)  STR_(x)
//      STR(FW_MAJOR)              // "3"
//
//  Token pasting has exactly the same shape. `adc_ch##n##_raw` pastes
//  whatever tokens `n` literally is: pass it `1` and you get adc_ch1_raw;
//  pass it a macro CURRENT_CHANNEL and you get adc_chCURRENT_CHANNEL_raw --
//  the undeclared identifier in your error message. Same ladder, same fix.
//
//  Use pasting in moderation. An identifier assembled by ## exists nowhere
//  in the source, so grep cannot find it and neither can most IDEs -- a real
//  cost in a code base where "who touches this register?" is a debugging
//  question. Vendor headers (CMSIS among them) accept that cost to define
//  thousands of register accessors; you should accept it about that
//  reluctantly.
//
//  TASK
//    Give STR and ADC_RAW the two-layer indirection they need. The tests
//    pin down both the literal and the through-a-macro cases.
//
//  RUN IT
//    ./mec test 07_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

#define FW_MAJOR 3
#define FW_MINOR 14

// TODO: stringifies its argument's NAME; the version string comes out as
// "vFW_MAJOR.FW_MINOR".
#define STR(x) #x

#define FW_VERSION "v" STR(FW_MAJOR) "." STR(FW_MINOR)

static const uint16_t adc_ch0_raw = 101;
static const uint16_t adc_ch1_raw = 202;
static const uint16_t adc_ch2_raw = 303;

// TODO: pastes its argument's NAME; fine for ADC_RAW(1), a compile error
// for ADC_RAW(CURRENT_CHANNEL).
#define ADC_RAW(n) adc_ch##n##_raw

#define CURRENT_CHANNEL 1

TEST("the version string carries numbers, not macro names") {
  CHECK_EQ(FW_VERSION, "v3.14");
}

TEST("stringify still works on a literal") {
  CHECK_EQ(STR(115200), "115200");
}

TEST("token pasting reaches the channel variables") {
  CHECK_EQ(ADC_RAW(0), 101u);
  CHECK_EQ(ADC_RAW(2), 303u);
}

TEST("pasting works when the channel is itself a macro") {
  CHECK_EQ(ADC_RAW(CURRENT_CHANNEL), 202u);
}
