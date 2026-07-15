#ifndef MAKITA_OBI_H
#define MAKITA_OBI_H

#include <stdint.h>

#define MAKITA_OBI_ROM_SIZE 8U
#define MAKITA_OBI_MODEL_LENGTH 8U
#define MAKITA_OBI_CELL_COUNT 5U
#define MAKITA_OBI_STATUS_RAW_SIZE 48U

typedef enum
{
  MAKITA_OBI_OK = 0,
  MAKITA_OBI_NO_BATTERY,
  MAKITA_OBI_BAD_RESPONSE,
  MAKITA_OBI_INVALID_ARGUMENT
} MakitaObiResult;

typedef struct
{
  uint16_t pack_mv;
  uint16_t cell_mv[MAKITA_OBI_CELL_COUNT];
  uint16_t difference_mv;
} MakitaObiVoltages;

typedef struct
{
  uint16_t raw_tenths_kelvin;
  int16_t tenths_celsius;
} MakitaObiTemperature;

typedef struct
{
  uint8_t raw[MAKITA_OBI_STATUS_RAW_SIZE];
  uint8_t status_code;
  uint8_t locked;
  uint8_t capacity_tenths_ah;
  uint16_t charge_cycles;
  uint8_t manufacture_year;
  uint8_t manufacture_month;
  uint8_t manufacture_day;
} MakitaObiStatus;

void makita_obi_init(void);
MakitaObiResult makita_obi_read_rom(uint8_t rom[MAKITA_OBI_ROM_SIZE]);
MakitaObiResult makita_obi_read_model(char model[MAKITA_OBI_MODEL_LENGTH + 1U]);
MakitaObiResult makita_obi_read_voltages(MakitaObiVoltages *voltages);
MakitaObiResult makita_obi_read_temperature(MakitaObiTemperature *temperature);
MakitaObiResult makita_obi_read_status(MakitaObiStatus *status);
MakitaObiResult makita_obi_led_on(void);
MakitaObiResult makita_obi_led_off(void);
uint8_t makita_clear_errors(void);

#endif /* MAKITA_OBI_H */
