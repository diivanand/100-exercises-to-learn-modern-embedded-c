// =============================================================================
//  05.04 -- Tagged unions: the sum type you build yourself
// =============================================================================
//
//  An event in a firmware main loop is one of several shapes: a button
//  changed, an ADC conversion finished, a byte arrived. A struct carrying
//  every field of every shape pays for all of them in every event; a union
//  carries any ONE shape, at the cost of the largest. What a bare union
//  cannot say is WHICH shape it currently holds -- so C's idiom pairs it
//  with an enum tag, and the two travel together:
//
//      struct event {
//        enum event_type type;      // the tag: which member is live
//        union event_payload as;    // the payload: one member, ever
//      };
//
//  This is a sum type built by hand -- what other languages spell `enum`
//  (Rust) or `variant` (C++). The compiler enforces none of it: reading a
//  member other than the live one is legal C (05.03 made that a feature) and
//  quietly hands back reinterpreted bytes. Two disciplines stand in for the
//  missing checks:
//
//   1. CONSTRUCTION sets tag and payload together, in one place. The
//      event_button()/event_adc()/event_uart_byte() helpers below exist for
//      exactly that; events assembled field-by-field at call sites are where
//      mismatched tags come from.
//
//   2. CONSUMPTION switches on the tag, touches only the matching member,
//      and covers EVERY tag. Here the compiler finally can help: a switch
//      over an enum with NO default warns (-Wswitch -- an error in this
//      build) the day someone adds a tag and forgets a case. A `default:`
//      silences that guardian forever.
//
//  (MISRA C:2012 Rule 19.2 lists `union` as advisory-forbidden; overlaid
//  memory has burned enough firmware to earn that. The tagged pattern, held
//  to the two disciplines above, is the shape most shops carve out an
//  exception for.)
//
//  The starter's dispatcher has both classic wounds. A copy-paste bug reads
//  the WRONG MEMBER for ADC events -- no crash, no warning, just the channel
//  number wearing a millivolts name badge. And its `default:` swallows every
//  UART byte, silently, with -Wswitch disarmed.
//
//  TASK
//    Fix `status_apply`: read the right member for each tag, handle
//    EVENT_UART_BYTE, and remove the default so -Wswitch guards the future.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 05_04
//
// =============================================================================

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
  switch (e->type) {
  case EVENT_BUTTON:
    if (e->as.button.pressed) {
      s->button_presses++;
    }
    break;
  case EVENT_ADC_READING:
    // TODO: wrong member -- this reads the first byte of the union and
    // calls it millivolts.
    s->last_millivolts = e->as.byte;
    break;
  default:
    // TODO: EVENT_UART_BYTE lands here and vanishes -- and this default is
    // what keeps -Wswitch from ever mentioning it.
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
