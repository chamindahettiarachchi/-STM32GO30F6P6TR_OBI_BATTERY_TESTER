#ifndef ST7567_H
#define ST7567_H

#include <stdint.h>

#define LCD_WIDTH 132U
#define LCD_PAGE_COUNT 4U

void lcd_init(void);
void lcd_clear(void);
void lcd_test_fill(void);
void lcd_char(uint8_t x, uint8_t page, char c);
void lcd_string(uint8_t x, uint8_t page, const char *text);
void lcd_set_contrast(uint8_t contrast);
void lcd_display_on(uint8_t enabled);

#endif /* ST7567_H */
