#include "obi_ui.h"

#include <stddef.h>

#include "makita_obi.h"
#include "st7567.h"

#define OBI_UI_ITEM_COUNT 8U
#define OBI_UI_LINE_SIZE 20U

enum
{
  OBI_UI_ROM = 0,
  OBI_UI_MODEL,
  OBI_UI_VOLTAGES,
  OBI_UI_TEMPERATURE,
  OBI_UI_STATUS,
  OBI_UI_LED_ON,
  OBI_UI_LED_OFF,
  OBI_UI_CLEAR_ERRORS
};

static const char *const menu_names[OBI_UI_ITEM_COUNT] =
{
  "ROM ID",
  "MODEL",
  "CELL VOLTAGES",
  "TEMPERATURE",
  "STATUS",
  "LED ON",
  "LED OFF",
  "CLEAR ERRORS"
};

static uint8_t selected_item;

static void line_clear(char *line)
{
  uint8_t i;
  for (i = 0U; i < OBI_UI_LINE_SIZE; i++)
  {
    line[i] = '\0';
  }
}

static void line_append_char(char *line, uint8_t *position, char value)
{
  if (*position < (OBI_UI_LINE_SIZE - 1U))
  {
    line[*position] = value;
    (*position)++;
    line[*position] = '\0';
  }
}

static void line_append_text(char *line, uint8_t *position, const char *text)
{
  while ((text != NULL) && (*text != '\0'))
  {
    line_append_char(line, position, *text);
    text++;
  }
}

static void line_append_u16(char *line, uint8_t *position, uint16_t value,
                            uint8_t minimum_digits)
{
  char digits[5];
  uint8_t count = 0U;

  do
  {
    digits[count] = (char)('0' + (value % 10U));
    value /= 10U;
    count++;
  } while ((value != 0U) && (count < sizeof(digits)));

  while ((count < minimum_digits) && (count < sizeof(digits)))
  {
    digits[count] = '0';
    count++;
  }
  while (count > 0U)
  {
    count--;
    line_append_char(line, position, digits[count]);
  }
}

static void line_append_hex(char *line, uint8_t *position, uint8_t value)
{
  static const char hex_digits[] = "0123456789ABCDEF";
  line_append_char(line, position, hex_digits[value >> 4U]);
  line_append_char(line, position, hex_digits[value & 0x0FU]);
}

static void line_append_mv(char *line, uint8_t *position, uint16_t millivolts)
{
  line_append_u16(line, position, (uint16_t)(millivolts / 1000U), 1U);
  line_append_char(line, position, '.');
  line_append_u16(line, position, (uint16_t)(millivolts % 1000U), 3U);
}

static void obi_ui_show_menu(void)
{
  lcd_clear();
  lcd_string(13U, 0U, "OBI BATTERY TEST");
  lcd_string(0U, 1U, ">");
  lcd_string(14U, 1U, menu_names[selected_item]);
  lcd_string(0U, 2U, "TURN: SELECT");
  lcd_string(0U, 3U, "PRESS: READ");
}

static void obi_ui_show_error(MakitaObiResult result)
{
  lcd_clear();
  lcd_string(0U, 0U, menu_names[selected_item]);
  if (result == MAKITA_OBI_NO_BATTERY)
  {
    lcd_string(0U, 2U, "NO BATTERY");
  }
  else if (result == MAKITA_OBI_BAD_RESPONSE)
  {
    lcd_string(0U, 2U, "BAD RESPONSE");
  }
  else
  {
    lcd_string(0U, 2U, "READ ERROR");
  }
  lcd_string(0U, 3U, "PRESS: RETRY");
}

static void obi_ui_read_rom(void)
{
  uint8_t rom[MAKITA_OBI_ROM_SIZE];
  MakitaObiResult result = makita_obi_read_rom(rom);
  char line[OBI_UI_LINE_SIZE];
  uint8_t row;

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }

  lcd_clear();
  lcd_string(0U, 0U, "ROM ID");
  for (row = 0U; row < 2U; row++)
  {
    uint8_t i;
    uint8_t position = 0U;
    line_clear(line);
    for (i = 0U; i < 4U; i++)
    {
      if (i != 0U)
      {
        line_append_char(line, &position, ' ');
      }
      line_append_hex(line, &position, rom[(row * 4U) + i]);
    }
    lcd_string(0U, (uint8_t)(row + 1U), line);
  }
  lcd_string(0U, 3U, "PRESS: REFRESH");
}

static void obi_ui_read_model(void)
{
  char model[MAKITA_OBI_MODEL_LENGTH + 1U];
  MakitaObiResult result = makita_obi_read_model(model);
  uint8_t i;

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }
  for (i = 0U; i < MAKITA_OBI_MODEL_LENGTH; i++)
  {
    if (((uint8_t)model[i] < 32U) || ((uint8_t)model[i] > 126U))
    {
      model[i] = ' ';
    }
  }

  lcd_clear();
  lcd_string(0U, 0U, "BATTERY MODEL");
  lcd_string(0U, 1U, model);
  lcd_string(0U, 3U, "PRESS: REFRESH");
}

static void obi_ui_voltage_pair(uint8_t page, uint8_t first_number,
                                uint16_t first_mv, uint8_t second_number,
                                uint16_t second_mv)
{
  char line[OBI_UI_LINE_SIZE];
  uint8_t position = 0U;

  line_clear(line);
  line_append_u16(line, &position, first_number, 1U);
  line_append_char(line, &position, ':');
  line_append_mv(line, &position, first_mv);
  line_append_char(line, &position, ' ');
  line_append_u16(line, &position, second_number, 1U);
  line_append_char(line, &position, ':');
  line_append_mv(line, &position, second_mv);
  lcd_string(0U, page, line);
}

static void obi_ui_read_voltages(void)
{
  MakitaObiVoltages voltages;
  MakitaObiResult result = makita_obi_read_voltages(&voltages);
  char line[OBI_UI_LINE_SIZE];
  uint8_t position = 0U;

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }

  lcd_clear();
  line_clear(line);
  line_append_text(line, &position, "P:");
  line_append_mv(line, &position, voltages.pack_mv);
  line_append_char(line, &position, 'V');
  lcd_string(0U, 0U, line);
  obi_ui_voltage_pair(1U, 1U, voltages.cell_mv[0], 2U, voltages.cell_mv[1]);
  obi_ui_voltage_pair(2U, 3U, voltages.cell_mv[2], 4U, voltages.cell_mv[3]);

  position = 0U;
  line_clear(line);
  line_append_text(line, &position, "5:");
  line_append_mv(line, &position, voltages.cell_mv[4]);
  line_append_text(line, &position, " D:");
  line_append_u16(line, &position, voltages.difference_mv, 1U);
  line_append_text(line, &position, "mV");
  lcd_string(0U, 3U, line);
}

static void obi_ui_read_temperature(void)
{
  MakitaObiTemperature temperature;
  MakitaObiResult result = makita_obi_read_temperature(&temperature);
  char line[OBI_UI_LINE_SIZE];
  uint8_t position = 0U;
  int16_t value;

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }

  lcd_clear();
  lcd_string(0U, 0U, "TEMPERATURE");
  line_clear(line);
  line_append_text(line, &position, "RAW:");
  line_append_u16(line, &position, temperature.raw_tenths_kelvin, 1U);
  lcd_string(0U, 1U, line);

  value = temperature.tenths_celsius;
  position = 0U;
  line_clear(line);
  line_append_text(line, &position, "TEMP:");
  if (value < 0)
  {
    line_append_char(line, &position, '-');
    value = (int16_t)-value;
  }
  line_append_u16(line, &position, (uint16_t)(value / 10), 1U);
  line_append_char(line, &position, '.');
  line_append_u16(line, &position, (uint16_t)(value % 10), 1U);
  line_append_char(line, &position, 'C');
  lcd_string(0U, 2U, line);
  lcd_string(0U, 3U, "PRESS: REFRESH");
}

static void obi_ui_read_status(void)
{
  MakitaObiStatus status;
  MakitaObiResult result = makita_obi_read_status(&status);
  char line[OBI_UI_LINE_SIZE];
  uint8_t position = 0U;

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }

  lcd_clear();
  line_clear(line);
  line_append_text(line, &position, (status.locked != 0U) ? "LOCKED S:" : "UNLOCKED S:");
  line_append_hex(line, &position, status.status_code);
  lcd_string(0U, 0U, line);

  position = 0U;
  line_clear(line);
  line_append_text(line, &position, "CAP:");
  line_append_u16(line, &position, (uint16_t)(status.capacity_tenths_ah / 10U), 1U);
  line_append_char(line, &position, '.');
  line_append_u16(line, &position, (uint16_t)(status.capacity_tenths_ah % 10U), 1U);
  line_append_text(line, &position, "Ah CY:");
  line_append_u16(line, &position, status.charge_cycles, 1U);
  lcd_string(0U, 1U, line);

  position = 0U;
  line_clear(line);
  line_append_text(line, &position, "DATE:20");
  line_append_u16(line, &position, status.manufacture_year, 2U);
  line_append_char(line, &position, '-');
  line_append_u16(line, &position, status.manufacture_month, 2U);
  line_append_char(line, &position, '-');
  line_append_u16(line, &position, status.manufacture_day, 2U);
  lcd_string(0U, 2U, line);
  lcd_string(0U, 3U, "PRESS: REFRESH");
}

static void obi_ui_set_led(uint8_t enabled)
{
  MakitaObiResult result = (enabled != 0U) ? makita_obi_led_on() :
                                                makita_obi_led_off();

  if (result != MAKITA_OBI_OK)
  {
    obi_ui_show_error(result);
    return;
  }

  lcd_clear();
  lcd_string(0U, 0U, "LED TEST");
  lcd_string(0U, 1U, (enabled != 0U) ? "LED ON OK" : "LED OFF OK");
  lcd_string(0U, 3U, "PRESS: REPEAT");
}

static void obi_ui_clear_errors_result(uint8_t success)
{
  lcd_clear();
  lcd_string(0U, 0U, "CLEAR ERRORS");
  if (success != 0U)
  {
    lcd_string(0U, 1U, "COMMAND SENT");
    lcd_string(0U, 2U, "READ STATUS");
  }
  else
  {
    lcd_string(0U, 1U, "FAILED");
    lcd_string(0U, 2U, "CHECK PINS");
  }
  lcd_string(0U, 3U, "PRESS: RETRY");
}

void obi_ui_init(void)
{
  selected_item = OBI_UI_ROM;
  obi_ui_show_menu();
}

void obi_ui_move(int8_t direction)
{
  if (direction > 0)
  {
    selected_item++;
    if (selected_item >= OBI_UI_ITEM_COUNT)
    {
      selected_item = 0U;
    }
  }
  else if (direction < 0)
  {
    selected_item = (selected_item == 0U) ? (OBI_UI_ITEM_COUNT - 1U) :
                                            (uint8_t)(selected_item - 1U);
  }
  obi_ui_show_menu();
}

void obi_ui_activate(void)
{
  lcd_clear();
  lcd_string(0U, 1U, "READING...");

  switch (selected_item)
  {
    case OBI_UI_ROM:
      obi_ui_read_rom();
      break;
    case OBI_UI_MODEL:
      obi_ui_read_model();
      break;
    case OBI_UI_VOLTAGES:
      obi_ui_read_voltages();
      break;
    case OBI_UI_TEMPERATURE:
      obi_ui_read_temperature();
      break;
    case OBI_UI_STATUS:
      obi_ui_read_status();
      break;
    case OBI_UI_LED_ON:
      obi_ui_set_led(1U);
      break;
    case OBI_UI_LED_OFF:
      obi_ui_set_led(0U);
      break;
    case OBI_UI_CLEAR_ERRORS:
      obi_ui_clear_errors_result(makita_clear_errors());
      break;
    default:
      obi_ui_show_menu();
      break;
  }
}
