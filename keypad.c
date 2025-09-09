#include "keypad.h"
#include <util/delay.h>

// Define the keypad layout
const char keys[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

void keypad_init() {
    // Set rows (PD0-PD3) as outputs, and columns (PD4-PD7) as inputs
    DDRD = 0x0F;
    // Enable pull-up resistors on columns
    PORTD = 0xF0;
}

char get_key() {
    for (uint8_t r = 0; r < 4; r++) {
        // Ground one row at a time
        PORTD = ~(1 << r);
        _delay_us(1); // Allow pins to settle

        for (uint8_t c = 0; c < 4; c++) {
            // Check if a column is pulled low by a key press
            if (!(PIND & (1 << (c + 4)))) {
                _delay_ms(20); // Simple debounce
                while (!(PIND & (1 << (c + 4)))); // Wait for key release
                return keys[r][c];
            }
        }
    }
    return 0; // No key pressed
}
