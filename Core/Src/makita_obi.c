/*
 * Makita OBI one-wire read-only diagnostics.
 * Timing and command sequences are adapted from:
 *   chamindahettiarachchi/Makita_OBI_Arduino_code
 *   chamindahettiarachchi/attiny1614_OBI_With_RESET_btn
 * and the STM32 test code supplied with this project.
 */
#include "makita_obi.h"

#include <stddef.h>
#include <string.h>

#include "main.h"

_Static_assert(ONE_WIRE_PIN_Pin == GPIO_PIN_3,
               "Update OBI_PIN_INDEX when moving ONE_WIRE_PIN away from pin 3.");

#define OBI_PIN_INDEX 3U
#define OBI_PIN_MODE_MASK (3UL << (OBI_PIN_INDEX * 2U))
#define OBI_PIN_OUTPUT_MODE (1UL << (OBI_PIN_INDEX * 2U))
#define OBI_ENABLE_STARTUP_MS 400U
#define OBI_COMMAND_GAP_US 90U
#define OBI_RESET_LOW_US 750U
#define OBI_RESET_SAMPLE_US 70U
#define OBI_RESET_RECOVERY_US 410U

static void obi_delay_us(uint16_t microseconds)
{
  uint32_t previous = SysTick->VAL;
  uint32_t elapsed = 0U;
  uint32_t reload = SysTick->LOAD + 1U;
  uint32_t ticks = (SystemCoreClock / 1000000UL) * microseconds;

  while (elapsed < ticks)
  {
    uint32_t current = SysTick->VAL;
    if (previous >= current)
    {
      elapsed += previous - current;
    }
    else
    {
      elapsed += previous + (reload - current);
    }
    previous = current;
  }
}

static void obi_drive_low(void)
{
  ONE_WIRE_PIN_GPIO_Port->BRR = ONE_WIRE_PIN_Pin;
  ONE_WIRE_PIN_GPIO_Port->MODER =
      (ONE_WIRE_PIN_GPIO_Port->MODER & ~OBI_PIN_MODE_MASK) | OBI_PIN_OUTPUT_MODE;
}

static void obi_release(void)
{
  ONE_WIRE_PIN_GPIO_Port->MODER &= ~OBI_PIN_MODE_MASK;
}

static uint8_t obi_read_pin(void)
{
  return ((ONE_WIRE_PIN_GPIO_Port->IDR & ONE_WIRE_PIN_Pin) != 0U) ? 1U : 0U;
}

static uint8_t obi_reset_pulse(void)
{
  uint8_t presence;

  obi_release();
  obi_delay_us(20U);
  obi_drive_low();
  obi_delay_us(OBI_RESET_LOW_US);
  obi_release();
  obi_delay_us(OBI_RESET_SAMPLE_US);
  presence = (obi_read_pin() == 0U) ? 1U : 0U;
  obi_delay_us(OBI_RESET_RECOVERY_US);

  return presence;
}

static void obi_write_bit(uint8_t value)
{
  obi_drive_low();
  if (value != 0U)
  {
    obi_delay_us(1U);
    obi_release();
    obi_delay_us(120U);
  }
  else
  {
    obi_delay_us(90U);
    obi_release();
    obi_delay_us(30U);
  }
}

static uint8_t obi_read_bit(void)
{
  uint8_t value;

  obi_drive_low();
  obi_delay_us(1U);
  obi_release();
  obi_delay_us(10U);
  value = obi_read_pin();
  obi_delay_us(55U);

  return value;
}

static void obi_write_byte(uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; bit++)
  {
    obi_write_bit((uint8_t)(value & 0x01U));
    value >>= 1U;
  }
}

static uint8_t obi_read_byte(void)
{
  uint8_t bit;
  uint8_t value = 0U;

  for (bit = 0U; bit < 8U; bit++)
  {
    if (obi_read_bit() != 0U)
    {
      value |= (uint8_t)(1U << bit);
    }
  }
  return value;
}

static void obi_power_on(void)
{
  HAL_GPIO_WritePin(ENABLE_PIN_GPIO_Port, ENABLE_PIN_Pin, GPIO_PIN_SET);
  HAL_Delay(OBI_ENABLE_STARTUP_MS);
}

static void obi_power_off(void)
{
  obi_release();
  HAL_GPIO_WritePin(ENABLE_PIN_GPIO_Port, ENABLE_PIN_Pin, GPIO_PIN_RESET);
}

static uint32_t obi_enter_critical(void)
{
  uint32_t previous_primask = __get_PRIMASK();
  __disable_irq();
  return previous_primask;
}

static void obi_exit_critical(uint32_t previous_primask)
{
  if (previous_primask == 0U)
  {
    __enable_irq();
  }
}

static uint8_t obi_buffer_is_invalid(const uint8_t *data, uint8_t length)
{
  uint8_t i;
  uint8_t all_zero = 1U;
  uint8_t all_ff = 1U;

  for (i = 0U; i < length; i++)
  {
    if (data[i] != 0x00U)
    {
      all_zero = 0U;
    }
    if (data[i] != 0xFFU)
    {
      all_ff = 0U;
    }
  }
  return ((all_zero != 0U) || (all_ff != 0U)) ? 1U : 0U;
}

static MakitaObiResult obi_command_cc(const uint8_t *command, uint8_t command_length,
                                      uint8_t *response, uint8_t response_length)
{
  uint8_t i;
  uint32_t primask = obi_enter_critical();

  if (obi_reset_pulse() == 0U)
  {
    obi_exit_critical(primask);
    return MAKITA_OBI_NO_BATTERY;
  }

  obi_delay_us(400U);
  obi_write_byte(0xCCU);
  for (i = 0U; i < command_length; i++)
  {
    obi_delay_us(OBI_COMMAND_GAP_US);
    obi_write_byte(command[i]);
  }
  for (i = 0U; i < response_length; i++)
  {
    obi_delay_us(OBI_COMMAND_GAP_US);
    response[i] = obi_read_byte();
  }

  obi_exit_critical(primask);
  return MAKITA_OBI_OK;
}

static MakitaObiResult obi_command_33(const uint8_t *command, uint8_t command_length,
                                      uint8_t *response, uint8_t response_length)
{
  uint8_t i;
  uint32_t primask = obi_enter_critical();

  if (obi_reset_pulse() == 0U)
  {
    obi_exit_critical(primask);
    return MAKITA_OBI_NO_BATTERY;
  }

  obi_delay_us(400U);
  obi_write_byte(0x33U);
  for (i = 0U; i < MAKITA_OBI_ROM_SIZE; i++)
  {
    obi_delay_us(OBI_COMMAND_GAP_US);
    response[i] = obi_read_byte();
  }
  for (i = 0U; i < command_length; i++)
  {
    obi_delay_us(OBI_COMMAND_GAP_US);
    obi_write_byte(command[i]);
  }
  for (i = 0U; i < response_length; i++)
  {
    obi_delay_us(OBI_COMMAND_GAP_US);
    response[MAKITA_OBI_ROM_SIZE + i] = obi_read_byte();
  }

  obi_exit_critical(primask);
  return MAKITA_OBI_OK;
}

static uint8_t obi_nibble_swap(uint8_t value)
{
  return (uint8_t)((value << 4U) | (value >> 4U));
}

void makita_obi_init(void)
{
  HAL_GPIO_WritePin(ENABLE_PIN_GPIO_Port, ENABLE_PIN_Pin, GPIO_PIN_RESET);
  obi_release();
}

MakitaObiResult makita_obi_read_rom(uint8_t rom[MAKITA_OBI_ROM_SIZE])
{
  MakitaObiResult result;

  if (rom == NULL)
  {
    return MAKITA_OBI_INVALID_ARGUMENT;
  }

  memset(rom, 0xFF, MAKITA_OBI_ROM_SIZE);
  obi_power_on();
  result = obi_command_33(NULL, 0U, rom, 0U);
  obi_power_off();

  if ((result == MAKITA_OBI_OK) &&
      (obi_buffer_is_invalid(rom, MAKITA_OBI_ROM_SIZE) != 0U))
  {
    result = MAKITA_OBI_BAD_RESPONSE;
  }
  return result;
}

MakitaObiResult makita_obi_read_model(char model[MAKITA_OBI_MODEL_LENGTH + 1U])
{
  static const uint8_t command[] = {0xDCU, 0x0CU};
  uint8_t response[16];
  MakitaObiResult result;

  if (model == NULL)
  {
    return MAKITA_OBI_INVALID_ARGUMENT;
  }

  memset(model, 0, MAKITA_OBI_MODEL_LENGTH + 1U);
  memset(response, 0xFF, sizeof(response));
  obi_power_on();
  result = obi_command_cc(command, sizeof(command), response, sizeof(response));
  obi_power_off();

  if ((result == MAKITA_OBI_OK) &&
      (obi_buffer_is_invalid(response, sizeof(response)) != 0U))
  {
    return MAKITA_OBI_BAD_RESPONSE;
  }
  if (result == MAKITA_OBI_OK)
  {
    memcpy(model, response, MAKITA_OBI_MODEL_LENGTH);
    model[MAKITA_OBI_MODEL_LENGTH] = '\0';
  }
  return result;
}

MakitaObiResult makita_obi_read_voltages(MakitaObiVoltages *voltages)
{
  static const uint8_t model_command[] = {0xDCU, 0x0CU};
  static const uint8_t voltage_command[] = {0xD7U, 0x00U, 0x00U, 0xFFU};
  uint8_t warmup_response[16];
  uint8_t response[30];
  uint8_t i;
  uint16_t minimum;
  uint16_t maximum;
  MakitaObiResult result;

  if (voltages == NULL)
  {
    return MAKITA_OBI_INVALID_ARGUMENT;
  }

  memset(voltages, 0, sizeof(*voltages));
  memset(response, 0xFF, sizeof(response));
  obi_power_on();
  result = obi_command_cc(model_command, sizeof(model_command),
                          warmup_response, sizeof(warmup_response));
  if (result == MAKITA_OBI_OK)
  {
    HAL_Delay(50U);
    result = obi_command_cc(voltage_command, sizeof(voltage_command),
                            response, sizeof(response));
  }
  obi_power_off();

  if ((result == MAKITA_OBI_OK) && (obi_buffer_is_invalid(response, 12U) != 0U))
  {
    return MAKITA_OBI_BAD_RESPONSE;
  }
  if (result != MAKITA_OBI_OK)
  {
    return result;
  }

  voltages->pack_mv = (uint16_t)response[0] | ((uint16_t)response[1] << 8U);
  for (i = 0U; i < MAKITA_OBI_CELL_COUNT; i++)
  {
    uint8_t offset = (uint8_t)(2U + (i * 2U));
    voltages->cell_mv[i] = (uint16_t)response[offset] |
                           ((uint16_t)response[offset + 1U] << 8U);
  }

  minimum = voltages->cell_mv[0];
  maximum = voltages->cell_mv[0];
  for (i = 1U; i < MAKITA_OBI_CELL_COUNT; i++)
  {
    if (voltages->cell_mv[i] < minimum)
    {
      minimum = voltages->cell_mv[i];
    }
    if (voltages->cell_mv[i] > maximum)
    {
      maximum = voltages->cell_mv[i];
    }
  }
  voltages->difference_mv = (uint16_t)(maximum - minimum);
  return MAKITA_OBI_OK;
}

MakitaObiResult makita_obi_read_temperature(MakitaObiTemperature *temperature)
{
  static const uint8_t model_command[] = {0xDCU, 0x0CU};
  static const uint8_t temperature_command[] = {0xD7U, 0x0EU, 0x00U, 0x02U};
  uint8_t warmup_response[16];
  uint8_t response[3] = {0xFFU, 0xFFU, 0xFFU};
  MakitaObiResult result;

  if (temperature == NULL)
  {
    return MAKITA_OBI_INVALID_ARGUMENT;
  }

  memset(temperature, 0, sizeof(*temperature));
  obi_power_on();
  result = obi_command_cc(model_command, sizeof(model_command),
                          warmup_response, sizeof(warmup_response));
  if (result == MAKITA_OBI_OK)
  {
    HAL_Delay(50U);
    result = obi_command_cc(temperature_command, sizeof(temperature_command),
                            response, sizeof(response));
  }
  obi_power_off();

  if ((result == MAKITA_OBI_OK) && (obi_buffer_is_invalid(response, 2U) != 0U))
  {
    return MAKITA_OBI_BAD_RESPONSE;
  }
  if (result == MAKITA_OBI_OK)
  {
    temperature->raw_tenths_kelvin =
        (uint16_t)response[0] | ((uint16_t)response[1] << 8U);
    temperature->tenths_celsius =
        (int16_t)((int32_t)temperature->raw_tenths_kelvin - 2731L);
  }
  return result;
}

MakitaObiResult makita_obi_read_status(MakitaObiStatus *status)
{
  static const uint8_t model_command[] = {0xDCU, 0x0CU};
  static const uint8_t command[] = {0xAAU, 0x00U};
  uint8_t warmup_response[16];
  MakitaObiResult result;

  if (status == NULL)
  {
    return MAKITA_OBI_INVALID_ARGUMENT;
  }

  memset(status, 0, sizeof(*status));
  memset(status->raw, 0xFF, sizeof(status->raw));
  obi_power_on();
  result = obi_command_cc(model_command, sizeof(model_command),
                          warmup_response, sizeof(warmup_response));
  if (result == MAKITA_OBI_OK)
  {
    HAL_Delay(50U);
    result = obi_command_33(command, sizeof(command), status->raw, 40U);
  }
  obi_power_off();

  if ((result == MAKITA_OBI_OK) &&
      (obi_buffer_is_invalid(&status->raw[MAKITA_OBI_ROM_SIZE], 40U) != 0U))
  {
    return MAKITA_OBI_BAD_RESPONSE;
  }
  if (result == MAKITA_OBI_OK)
  {
    status->status_code = status->raw[27];
    status->locked = ((status->raw[28] & 0x0FU) != 0U) ? 1U : 0U;
    status->capacity_tenths_ah = obi_nibble_swap(status->raw[24]);
    status->charge_cycles =
        (uint16_t)((((uint16_t)obi_nibble_swap(status->raw[34]) << 8U) |
                    obi_nibble_swap(status->raw[35])) & 0x0FFFU);
    status->manufacture_year = status->raw[0];
    status->manufacture_month = status->raw[1];
    status->manufacture_day = status->raw[2];
  }
  return result;
}
