#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdint.h>

typedef struct
{
  int8_t rotation;
  uint8_t button_pressed;
} RotaryEncoderEvent;

void rotary_encoder_init(void);
RotaryEncoderEvent rotary_encoder_poll(void);

#endif /* ROTARY_ENCODER_H */
