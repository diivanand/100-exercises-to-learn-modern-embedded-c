// Solution -- 05.04 Tagged unions: the sum type you build yourself

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

enum event_type {
  EVENT_BUTTON,
  EVENT_ADC_READING,
  EVENT_UART_BYTE,
};

union event_payload {
  struct {
    uint8_t id;
    bool pressed;
  } button;
  struct {
    uint8_t channel;
    uint16_t millivolts;
  } adc;
  uint8_t byte;
};

struct event {
  enum event_type type;   // the tag: which member of `as` is live
  union event_payload as; // the payload: one member, ever
};

// Constructors: the tag and its payload are set together or not at all.
static struct event event_button(uint8_t id, bool pressed) {
  return (struct event){.type = EVENT_BUTTON,
                        .as.button = {.id = id, .pressed = pressed}};
}

static struct event event_adc(uint8_t channel, uint16_t millivolts) {
  return (struct event){.type = EVENT_ADC_READING,
                        .as.adc = {.channel = channel, .millivolts = millivolts}};
}

static struct event event_uart_byte(uint8_t byte) {
  return (struct event){.type = EVENT_UART_BYTE, .as.byte = byte};
}

struct status {
  uint32_t button_presses;
  uint16_t last_millivolts;
  uint8_t last_byte;
};

void status_apply(struct status *s, const struct event *e) {
  // Every tag its own case, every case its own member -- and NO default, so
  // the day a fourth event type appears, -Wswitch (an error in this build)
  // refuses to look away until this switch says what to do about it.
  switch (e->type) {
  case EVENT_BUTTON:
    if (e->as.button.pressed) {
      s->button_presses++;
    }
    break;
  case EVENT_ADC_READING:
    s->last_millivolts = e->as.adc.millivolts;
    break;
  case EVENT_UART_BYTE:
    s->last_byte = e->as.byte;
    break;
  }
}

TEST("button presses are counted, releases are not") {
  struct status s = {0};
  const struct event press = event_button(0, true);
  const struct event release = event_button(0, false);
  status_apply(&s, &press);
  status_apply(&s, &release);
  status_apply(&s, &press);
  CHECK_EQ(s.button_presses, 2u);
}

TEST("an ADC event updates the reading -- from the right member") {
  struct status s = {0};
  const struct event e = event_adc(2, 3000);
  status_apply(&s, &e);
  // A wrong-member read does not crash and does not warn: it hands back
  // reinterpreted bytes. Here they would be the channel number.
  CHECK_EQ(s.last_millivolts, 3000u);
}

TEST("no event type is silently dropped") {
  struct status s = {0};
  const struct event e = event_uart_byte(0x55);
  status_apply(&s, &e);
  CHECK_EQ(s.last_byte, 0x55u);
}

TEST("the union costs its largest member, not the sum") {
  // button is 2 bytes, adc is 4, byte is 1: together 7, overlaid 4.
  CHECK_EQ(sizeof(union event_payload), 4u);
}
