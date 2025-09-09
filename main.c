#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include "i2c_lcd.h"
#include "mfrc522.h"

#define F_CPU 8000000UL
#define LCD_ADDRESS 0x27
#define MAX_CARDS 10 // Maximum number of cards we can save

// --- Function Prototypes ---
void save_uid_to_eeprom(uint8_t *uid);
uint8_t is_uid_saved(uint8_t *uid_to_check);
void initialize_eeprom();

void spi_init_master(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << PB2);
    DDRB &= ~(1 << PB4);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
}

int main(void) {
    uint8_t card_uid[5];
    char buffer[17];

    spi_init_master();
    lcd_init(LCD_ADDRESS);
    MFRC522_Init();
    
    // Check and initialize EEPROM on first run
    initialize_eeprom();

    while (1) {
        lcd_clear();
        lcd_print_string("Scan Your Card");
        uint8_t card_count = eeprom_read_byte((uint8_t*)0);
        sprintf(buffer, "Saved: %d/%d", card_count, MAX_CARDS);
        lcd_set_cursor(0, 1);
        lcd_print_string(buffer);

        if (MFRC522_IsCardPresent() && MFRC522_ReadCardSerial(card_uid)) {
            
            if (is_uid_saved(card_uid)) {
                lcd_clear();
                lcd_print_string("Access Granted!");
                _delay_ms(2000);
            } else {
                lcd_clear();
                lcd_print_string("Access Denied");
                _delay_ms(1500);

                lcd_clear();
                lcd_print_string("New Card Found");
                lcd_set_cursor(0, 1);
                lcd_print_string("Saving...");
                _delay_ms(1000);
                
                save_uid_to_eeprom(card_uid);
            }
        }
        _delay_ms(500);
    }
}

// --- EEPROM Helper Functions ---

/**
 * @brief Checks if the EEPROM has been initialized. If not, it resets the card count.
 */
void initialize_eeprom() {
    uint8_t count = eeprom_read_byte((uint8_t*)0);
    if (count > MAX_CARDS) { // If count is 0xFF or other garbage
        lcd_clear();
        lcd_print_string("First Run Setup");
        lcd_set_cursor(0, 1);
        lcd_print_string("Initializing...");
        eeprom_write_byte((uint8_t*)0, 0); // Reset card count to zero
        _delay_ms(1500);
    }
}

uint8_t is_uid_saved(uint8_t *uid_to_check) {
    uint8_t saved_card_count = eeprom_read_byte((uint8_t*)0);
    uint8_t stored_uid[4];

    for (uint8_t i = 0; i < saved_card_count; i++) {
        eeprom_read_block(stored_uid, (const void*)(1 + (i * 4)), 4);
        if (memcmp(uid_to_check, stored_uid, 4) == 0) {
            return 1; // Match found
        }
    }
    return 0; // No match
}

void save_uid_to_eeprom(uint8_t *uid) {
    if (is_uid_saved(uid)) {
        return;
    }

    uint8_t saved_card_count = eeprom_read_byte((uint8_t*)0);

    if (saved_card_count < MAX_CARDS) {
        uint16_t new_address = 1 + (saved_card_count * 4);
        eeprom_write_block(uid, (void*)new_address, 4);
        eeprom_write_byte((uint8_t*)0, saved_card_count + 1);

        lcd_clear();
        lcd_print_string("Card Saved!");
        _delay_ms(1500);
    } else {
        lcd_clear();
        lcd_print_string("Memory Full!");
        _delay_ms(1500);
    }
}