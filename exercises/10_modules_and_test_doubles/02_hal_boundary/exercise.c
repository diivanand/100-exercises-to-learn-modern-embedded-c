// =============================================================================
//  10.02 -- One boundary, no leaks: the HAL
// =============================================================================
//
//  10.01 injected one register. Real firmware touches dozens, and injecting
//  them one by one does not scale. The next size up is the HARDWARE
//  ABSTRACTION LAYER: one struct of function pointers is the only door
//  between application logic and silicon (Grenning ch. 5; ch. 11 grows the
//  same idea into full interfaces).
//
//      struct hal {
//        uint32_t (*read_adc)(uint8_t channel);
//        void     (*set_pwm)(uint8_t channel, uint16_t duty);
//        uint32_t (*millis)(void);
//      };
//
//  Application code takes `const struct hal *` and knows nothing else. The
//  board wires it to real peripherals; the tests wire it to doubles: a FAKE
//  for the input (the test decides what temperature the "sensor" reads) and
//  a SPY for the output (the test records what duty was commanded). This is
//  also the honest alternative to per-target #ifdef forests (07.04): one
//  boundary, two implementations, zero conditional compilation.
//
//  A boundary only works if NOTHING goes around it. The starter's fan
//  controller has a leak of the most lifelike kind: during board bring-up,
//  somebody bolted an "overheat guard" straight onto `board_read_adc()` --
//  the real hardware function -- instead of the injected interface. On the
//  bench it seemed fine. Under test, the fake sets the temperature to 85.0
//  degrees and the guard serenely reads the bench's 25.0, because the fake
//  never sees the call. One bypassed pointer and the whole seam is
//  ornamental: the test suite can no longer say anything about the one path
//  that matters most.
//
//  (Grenning ch. 6, "We Have to Interact with Hardware", answers exactly
//  this objection: you still test the LOGIC off-target; the thin wired
//  layer below the boundary is verified once, on the board -- chapter 15
//  here.)
//
//  TASK
//    Make the controller read the temperature exactly once, through the
//    HAL, and fold the overheat rung into the ladder. Do not change the
//    tests, and leave board.c alone -- production wiring is not the bug.
//
//  RUN IT
//    ./mec test 10_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// --- hal.h ----------------------------------------------------------------------

struct hal {
  uint32_t (*read_adc)(uint8_t channel);
  void (*set_pwm)(uint8_t channel, uint16_t duty);
  uint32_t (*millis)(void); // unused here; rate limiting arrives in ch. 13
};

// --- board.c: the production wiring (never runs in this test binary) -------------

uint32_t board_read_adc(uint8_t channel);
void board_set_pwm(uint8_t channel, uint16_t duty);
uint32_t board_millis(void);

uint32_t board_read_adc(uint8_t channel) {
  (void)channel;
  return 250; // stands in for "whatever the silicon says"; here, a cool bench
}

void board_set_pwm(uint8_t channel, uint16_t duty) {
  (void)channel;
  (void)duty; // on the board: a timer compare register
}

uint32_t board_millis(void) {
  return 0; // on the board: the SysTick counter
}

const struct hal board_hal = {board_read_adc, board_set_pwm, board_millis};

// --- fan_controller module ------------------------------------------------------

enum { TEMP_ADC_CHANNEL = 2, FAN_PWM_CHANNEL = 0 };

// Raw ADC thresholds (tenths of a degree at this divider): 40.0, 60.0, 80.0 C.
enum { RAW_WARM = 400, RAW_HOT = 600, RAW_OVERHEAT = 800 };
enum { DUTY_OFF = 0, DUTY_LOW = 300, DUTY_MID = 600, DUTY_FULL = 1000 };

void fan_controller_step(const struct hal *hal) {
  // TODO: this "guard" reads the BOARD, not the injected hal. The fake in
  // the tests never sees the call, so under test the overheat path is dead
  // code. (Added during bring-up; it even worked, once.)
  if (board_read_adc(TEMP_ADC_CHANNEL) >= RAW_OVERHEAT) {
    hal->set_pwm(FAN_PWM_CHANNEL, DUTY_FULL);
    return;
  }

  const uint32_t raw = hal->read_adc(TEMP_ADC_CHANNEL);

  uint16_t duty;
  if (raw >= RAW_HOT) {
    duty = DUTY_MID;
  } else if (raw >= RAW_WARM) {
    duty = DUTY_LOW;
  } else {
    duty = DUTY_OFF;
  }
  hal->set_pwm(FAN_PWM_CHANNEL, duty);
}

// --- tests: a hal made of doubles -------------------------------------------------

static uint32_t fake_adc_raw;
static uint8_t fake_adc_last_channel;
static uint16_t spy_pwm_duty;
static uint8_t spy_pwm_channel;
static unsigned spy_pwm_calls;

static uint32_t fake_read_adc(uint8_t channel) {
  fake_adc_last_channel = channel;
  return fake_adc_raw;
}

static void spy_set_pwm(uint8_t channel, uint16_t duty) {
  spy_pwm_channel = channel;
  spy_pwm_duty = duty;
  ++spy_pwm_calls;
}

static uint32_t fake_millis(void) {
  return 0;
}

static const struct hal test_hal = {fake_read_adc, spy_set_pwm, fake_millis};

static void step_at(uint32_t raw) {
  fake_adc_raw = raw;
  spy_pwm_calls = 0;
  fan_controller_step(&test_hal);
}

TEST("cool room: fan off, correct channels used") {
  step_at(250);
  CHECK_EQ(spy_pwm_duty, 0u);
  CHECK_EQ(spy_pwm_channel, 0u);
  CHECK_EQ(fake_adc_last_channel, 2u);
  CHECK_EQ(spy_pwm_calls, 1u);
}

TEST("warm: low speed") {
  step_at(450);
  CHECK_EQ(spy_pwm_duty, 300u);
}

TEST("hot: mid speed") {
  step_at(650);
  CHECK_EQ(spy_pwm_duty, 600u);
}

TEST("overheat: full speed -- the controller must learn it from OUR fake") {
  step_at(850);
  CHECK_EQ(spy_pwm_duty, 1000u);
}

TEST("threshold edges belong to the higher band") {
  step_at(400);
  CHECK_EQ(spy_pwm_duty, 300u);
  step_at(800);
  CHECK_EQ(spy_pwm_duty, 1000u);
}
