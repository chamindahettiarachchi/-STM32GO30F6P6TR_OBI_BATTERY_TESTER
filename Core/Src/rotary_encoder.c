#include "rotary_encoder.h"

#include "main.h"

#define ROTARY_COUNTS_PER_DETENT 4
#define ROTARY_BUTTON_DEBOUNCE_MS 20U

/* Valid quadrature transitions add or subtract one quarter-step. */
static const int8_t transition_table[16] =
{
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

static uint8_t previous_encoder_state;
static int8_t encoder_accumulator;
static uint8_t button_candidate;
static uint8_t button_stable;
static uint32_t button_change_time;

static uint8_t rotary_read_state(void)
{
  uint8_t s1 = (HAL_GPIO_ReadPin(ROTERY_S1_GPIO_Port, ROTERY_S1_Pin) == GPIO_PIN_SET) ? 1U : 0U;
  uint8_t s2 = (HAL_GPIO_ReadPin(ROTERY_S2_GPIO_Port, ROTERY_S2_Pin) == GPIO_PIN_SET) ? 1U : 0U;

  return (uint8_t)((s1 << 1U) | s2);
}

static uint8_t rotary_button_is_pressed(void)
{
  return (HAL_GPIO_ReadPin(ROTERY_KEY_GPIO_Port, ROTERY_KEY_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

void rotary_encoder_init(void)
{
  previous_encoder_state = rotary_read_state();
  encoder_accumulator = 0;
  button_candidate = rotary_button_is_pressed();
  button_stable = button_candidate;
  button_change_time = HAL_GetTick();
}

RotaryEncoderEvent rotary_encoder_poll(void)
{
  RotaryEncoderEvent event = {0, 0U};
  uint8_t encoder_state = rotary_read_state();
  uint8_t transition = (uint8_t)((previous_encoder_state << 2U) | encoder_state);
  uint8_t button_raw = rotary_button_is_pressed();
  uint32_t now = HAL_GetTick();

  previous_encoder_state = encoder_state;
  encoder_accumulator = (int8_t)(encoder_accumulator + transition_table[transition]);

  if (encoder_accumulator >= ROTARY_COUNTS_PER_DETENT)
  {
    event.rotation = 1;
    encoder_accumulator = 0;
  }
  else if (encoder_accumulator <= -ROTARY_COUNTS_PER_DETENT)
  {
    event.rotation = -1;
    encoder_accumulator = 0;
  }

  if (button_raw != button_candidate)
  {
    button_candidate = button_raw;
    button_change_time = now;
  }
  else if ((button_candidate != button_stable) &&
           ((uint32_t)(now - button_change_time) >= ROTARY_BUTTON_DEBOUNCE_MS))
  {
    button_stable = button_candidate;
    if (button_stable != 0U)
    {
      event.button_pressed = 1U;
    }
  }

  return event;
}
