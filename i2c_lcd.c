#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include "i2c_lcd.h"

// I2C Functions (based on AVR TWI)
void i2c_init(void);
void i2c_start(void);
void i2c_stop(void);
void i2c_write(uint8_t data);

// LCD specific functions
static uint8_t _lcd_address;
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);

void lcd_init(uint8_t address) {
    _lcd_address = address;
    i2c_init();

    _delay_ms(50);
    lcd_send_cmd(0x02); // Return home
    _delay_us(2000);
    lcd_send_cmd(0x28); // 4-bit mode, 2 line, 5x8 dots
    _delay_us(50);
    lcd_send_cmd(0x0C); // Display on, cursor off, blink off
    _delay_us(50);
    lcd_send_cmd(0x06); // Increment cursor
    _delay_us(50);
    lcd_clear();
    _delay_us(2000);
}

void lcd_clear(void) {
    lcd_send_cmd(0x01); // Clear display
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_send_cmd(0x80 | (col + row_offsets[row]));
}

void lcd_print_string(const char *str) {
    while (*str) {
        lcd_send_data(*str++);
    }
}

void lcd_print_int(int number) {
    char buffer[7]; // Max 6 digits for int + null terminator
    itoa(number, buffer, 10);
    lcd_print_string(buffer);
}

// Low-level functions
void lcd_send_cmd(uint8_t cmd) {
    uint8_t upper_nibble = (cmd & 0xF0) | 0x0C; // data, E=1, BKL=1
    uint8_t lower_nibble = ((cmd << 4) & 0xF0) | 0x0C;
    i2c_start();
    i2c_write(_lcd_address << 1);
    i2c_write(upper_nibble);
    i2c_write(upper_nibble & ~0x04); // E=0
    i2c_write(lower_nibble);
    i2c_write(lower_nibble & ~0x04); // E=0
    i2c_stop();
}

void lcd_send_data(uint8_t data) {
    uint8_t upper_nibble = (data & 0xF0) | 0x0D; // RS=1, E=1, BKL=1
    uint8_t lower_nibble = ((data << 4) & 0xF0) | 0x0D;
    i2c_start();
    i2c_write(_lcd_address << 1);
    i2c_write(upper_nibble);
    i2c_write(upper_nibble & ~0x04); // E=0
    i2c_write(lower_nibble);
    i2c_write(lower_nibble & ~0x04); // E=0
    i2c_stop();
}

// I2C/TWI implementation
void i2c_init(void) {
    TWSR = 0x00; // No prescaler
    TWBR = ((F_CPU / 100000L) - 16) / 2; // SCL frequency = 100kHz
}

void i2c_start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void i2c_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}
