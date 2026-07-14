/*
 * ST7567 132x32 LCD driver, adapted from:
 * https://github.com/chamindahettiarachchi/stm32g0-rtc-lcd-ui
 */
#include "st7567.h"

#include "font6x8.h"
#include "main.h"

#define LCD_CHARACTER_ADVANCE (FONT6X8_WIDTH + 1U)
#define LCD_MAX_CONTRAST 63U

#define LCD_RST_HIGH() HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_SET)
#define LCD_RST_LOW()  HAL_GPIO_WritePin(LCD_REST_GPIO_Port, LCD_REST_Pin, GPIO_PIN_RESET)
#define LCD_CD_HIGH()  HAL_GPIO_WritePin(LCD_CD_GPIO_Port, LCD_CD_Pin, GPIO_PIN_SET)
#define LCD_CD_LOW()   HAL_GPIO_WritePin(LCD_CD_GPIO_Port, LCD_CD_Pin, GPIO_PIN_RESET)
#define LCD_CS_HIGH()  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)
#define LCD_CS_LOW()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_SDA_HIGH() HAL_GPIO_WritePin(LCD_SDA_GPIO_Port, LCD_SDA_Pin, GPIO_PIN_SET)
#define LCD_SDA_LOW()  HAL_GPIO_WritePin(LCD_SDA_GPIO_Port, LCD_SDA_Pin, GPIO_PIN_RESET)
#define LCD_SCK_HIGH() HAL_GPIO_WritePin(LCD_SCK_GPIO_Port, LCD_SCK_Pin, GPIO_PIN_SET)
#define LCD_SCK_LOW()  HAL_GPIO_WritePin(LCD_SCK_GPIO_Port, LCD_SCK_Pin, GPIO_PIN_RESET)

static void lcd_write(uint8_t value, uint8_t is_data)
{
  uint8_t bit;

  if (is_data != 0U)
  {
    LCD_CD_HIGH();
  }
  else
  {
    LCD_CD_LOW();
  }

  LCD_CS_LOW();
  for (bit = 0U; bit < 8U; bit++)
  {
    LCD_SCK_LOW();
    if ((value & 0x80U) != 0U)
    {
      LCD_SDA_HIGH();
    }
    else
    {
      LCD_SDA_LOW();
    }
    value <<= 1U;
    LCD_SCK_HIGH();
  }
  LCD_SCK_LOW();
  LCD_CS_HIGH();
}

static void lcd_command(uint8_t command)
{
  lcd_write(command, 0U);
}

static void lcd_data(uint8_t data)
{
  lcd_write(data, 1U);
}

static void lcd_set_position(uint8_t x, uint8_t page)
{
  lcd_command((uint8_t)(0xB0U | page));
  lcd_command((uint8_t)(0x10U | (x >> 4U)));
  lcd_command((uint8_t)(x & 0x0FU));
}

static void lcd_fill(uint8_t pattern)
{
  uint8_t page;
  uint8_t column;

  for (page = 0U; page < LCD_PAGE_COUNT; page++)
  {
    lcd_set_position(0U, page);
    for (column = 0U; column < LCD_WIDTH; column++)
    {
      lcd_data(pattern);
    }
  }
}

void lcd_init(void)
{
  LCD_CS_HIGH();
  LCD_SCK_LOW();
  LCD_SDA_LOW();

  LCD_RST_HIGH();
  HAL_Delay(10U);
  LCD_RST_LOW();
  HAL_Delay(10U);
  LCD_RST_HIGH();
  HAL_Delay(10U);

  lcd_command(0xE2U); /* Software reset. */
  lcd_command(0x2CU); /* Voltage converter on. */
  lcd_command(0x2EU); /* Voltage regulator on. */
  lcd_command(0x2FU); /* Voltage follower on. */
  lcd_command(0xF8U); /* Booster ratio command. */
  lcd_command(0x00U); /* Four-times boost. */
  lcd_command(0x25U); /* Regulator resistor ratio. */
  lcd_set_contrast(0x20U);
  lcd_command(0xA2U); /* 1/9 bias. */
  lcd_command(0xC8U); /* Reverse COM scan direction. */
  lcd_command(0xA0U); /* Normal segment direction. */
  lcd_command(0xA6U); /* Normal (non-inverted) display. */
  lcd_display_on(1U);
}

void lcd_clear(void)
{
  lcd_fill(0x00U);
}

void lcd_test_fill(void)
{
  lcd_fill(0xFFU);
}

void lcd_char(uint8_t x, uint8_t page, char c)
{
  uint8_t column;
  uint8_t character = (uint8_t)c;

  if ((x >= LCD_WIDTH) || (page >= LCD_PAGE_COUNT))
  {
    return;
  }

  if ((character < FONT6X8_FIRST_CHAR) || (character > FONT6X8_LAST_CHAR))
  {
    character = (uint8_t)' ';
  }

  lcd_set_position(x, page);
  for (column = 0U; (column < FONT6X8_WIDTH) && (x < LCD_WIDTH); column++, x++)
  {
    lcd_data(Font6x8[character - FONT6X8_FIRST_CHAR][column]);
  }
  if (x < LCD_WIDTH)
  {
    lcd_data(0x00U);
  }
}

void lcd_string(uint8_t x, uint8_t page, const char *text)
{
  if ((text == NULL) || (page >= LCD_PAGE_COUNT))
  {
    return;
  }

  while ((*text != '\0') && (x < LCD_WIDTH))
  {
    lcd_char(x, page, *text);
    text++;
    if (x > (LCD_WIDTH - LCD_CHARACTER_ADVANCE))
    {
      break;
    }
    x = (uint8_t)(x + LCD_CHARACTER_ADVANCE);
  }
}

void lcd_set_contrast(uint8_t contrast)
{
  if (contrast > LCD_MAX_CONTRAST)
  {
    contrast = LCD_MAX_CONTRAST;
  }
  lcd_command(0x81U);
  lcd_command(contrast);
}

void lcd_display_on(uint8_t enabled)
{
  lcd_command((enabled != 0U) ? 0xAFU : 0xAEU);
}
