// Solution -- 07.03 Stringify, paste, and the indirection they both need

#include <mect/mect.h>

#include <stdint.h>

#define FW_MAJOR 3
#define FW_MINOR 14

// The two-layer ladder: STR's argument is macro-expanded on the way through,
// and only then does STR_ apply #. One layer alone stringifies the NAME.
#define STR_(x) #x
#define STR(x) STR_(x)

#define FW_VERSION "v" STR(FW_MAJOR) "." STR(FW_MINOR)

// Token pasting builds identifiers. Same indirection ladder, same reason:
// paste after expanding the argument, so ADC_RAW(CURRENT_CHANNEL) works as
// well as ADC_RAW(1).
static const uint16_t adc_ch0_raw = 101;
static const uint16_t adc_ch1_raw = 202;
static const uint16_t adc_ch2_raw = 303;

#define ADC_RAW_(n) adc_ch##n##_raw
#define ADC_RAW(n) ADC_RAW_(n)

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
