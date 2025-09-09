#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <inttypes.h>

void lcd_init(uint8_t address);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_print_string(const char *str);
void lcd_print_int(int number);

#endif // I2C_LCD_H
