// Solution -- 10.02 One boundary, no leaks: the HAL

#include <mect/mect.h>

#include <stdint.h>

// --- hal.h ----------------------------------------------------------------------

struct hal {
  uint32_t (*read_adc)(uint8_t channel);
  void (*set_pwm)(uint8_t channel, uint16_t duty);
  uint32_t (*millis)(void); // unused here; rate limiting arrives in ch. 13
};

// --- fan_controller module ------------------------------------------------------

enum { TEMP_ADC_CHANNEL = 2, FAN_PWM_CHANNEL = 0 };

// Raw ADC thresholds (tenths of a degree at this divider): 40.0, 60.0, 80.0 C.
enum { RAW_WARM = 400, RAW_HOT = 600, RAW_OVERHEAT = 800 };
enum { DUTY_OFF = 0, DUTY_LOW = 300, DUTY_MID = 600, DUTY_FULL = 1000 };

void fan_controller_step(const struct hal *hal) {
  // ONE read, through the boundary. Every path below decides from the same
  // sample -- including the overheat rung, which is just the top of the
  // ladder, not a special case with private access to the hardware.
  const uint32_t raw = hal->read_adc(TEMP_ADC_CHANNEL);

  uint16_t duty;
  if (raw >= RAW_OVERHEAT) {
    duty = DUTY_FULL;
  } else if (raw >= RAW_HOT) {
    duty = DUTY_MID;
  } else if (raw >= RAW_WARM) {
    duty = DUTY_LOW;
  } else {
    duty = DUTY_OFF;
  }
  hal->set_pwm(FAN_PWM_CHANNEL, duty);
}

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
