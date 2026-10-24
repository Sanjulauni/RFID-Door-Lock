/*
 * RFID + PIN door lock (ATmega328P @ 8 MHz)
 *
 * To open the door: scan a saved card, then type the PIN and press '#'.
 *
 * Keypad keys:
 *   0-9  PIN digits
 *   #    confirm
 *   *    clear what you typed
 *   D    cancel
 *   A    add a new card     (asks for the PIN first)
 *   B    change the PIN     (asks for the current PIN first)
 *   C    delete all cards   (asks for the PIN first)
 *
 * Wiring:
 *   Keypad rows PD0-PD3, columns PD4-PD7
 *   MFRC522 on SPI (SS = PB2, RST = PB1)
 *   LCD on I2C (PC4/PC5), address 0x27
 *   Lock relay/solenoid driver on PB0
 *
 * Three wrong PINs in a row lock the keypad and reader for 30 seconds.
 */

#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include "i2c_lcd.h"
#include "mfrc522.h"
#include "keypad.h"

#define LCD_ADDRESS 0x27
#define MAX_CARDS   10

// --- EEPROM layout ---
// Byte 0: number of saved cards
// Bytes 1-40: card IDs, 4 bytes each
// Bytes 48-51: PIN as ASCII digits
#define EE_CARD_COUNT   ((uint8_t *)0)
#define EE_CARDS_START  1
#define EE_PIN_ADDR     ((void *)48)
#define PIN_LENGTH      4
#define DEFAULT_PIN     "1234"

// --- Lock output ---
#define LOCK_DDR         DDRB
#define LOCK_PORT        PORTB
#define LOCK_PIN         PB0
#define LOCK_ACTIVE_HIGH 1      // Set to 0 for relay modules that switch on a LOW input
#define UNLOCK_TIME_MS   3000   // How long the door stays unlocked

// --- PIN entry ---
#define PIN_TIMEOUT_MS   10000  // Give up if no key is pressed for this long
#define CARD_TIMEOUT_MS  10000  // How long to wait for a card when adding one
#define MAX_WRONG_PINS   3
#define LOCKOUT_SECONDS  30

static uint8_t wrong_pin_count = 0;

// --- Function Prototypes ---
static void spi_init_master(void);
static void lock_init(void);
static void lock_set(uint8_t open);
static void unlock_door(void);
static void wait_ms(uint16_t ms);
static void show_message(const char *line1, const char *line2, uint16_t ms);
static void show_idle_screen(void);
static uint8_t read_pin(const char *prompt, char *pin);
static uint8_t verify_pin(const char *prompt);
static uint8_t wait_for_card(uint8_t *uid);
static void handle_card(uint8_t *uid);
static void add_card(void);
static void change_pin(void);
static void clear_cards(void);
static void initialize_eeprom(void);
static uint8_t is_uid_saved(uint8_t *uid_to_check);
static void save_uid_to_eeprom(uint8_t *uid);

int main(void) {
    uint8_t card_uid[5];
    uint8_t redraw = 1;
    uint8_t card_poll = 0;

    lock_init(); // First, so the door stays locked from power-up
    spi_init_master();
    keypad_init();
    lcd_init(LCD_ADDRESS);
    MFRC522_Init();
    initialize_eeprom();

    while (1) {
        // Only redraw when something changed, so the screen doesn't flicker
        if (redraw) {
            show_idle_screen();
            redraw = 0;
        }

        char key = get_key();
        if (key == 'A') {
            add_card();
            redraw = 1;
        } else if (key == 'B') {
            change_pin();
            redraw = 1;
        } else if (key == 'C') {
            clear_cards();
            redraw = 1;
        }

        // The keypad is checked every 10 ms, the card reader every 100 ms
        if (++card_poll >= 10) {
            card_poll = 0;
            if (MFRC522_IsCardPresent() && MFRC522_ReadCardSerial(card_uid)) {
                handle_card(card_uid);
                redraw = 1;
            }
        }

        _delay_ms(10);
    }
}

// --- Hardware setup ---

static void spi_init_master(void) {
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << PB2);
    DDRB &= ~(1 << PB4);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
}

static void lock_init(void) {
    LOCK_DDR |= (1 << LOCK_PIN);
    lock_set(0);
}

// open = 1 drives the relay/solenoid, open = 0 releases it
static void lock_set(uint8_t open) {
    uint8_t level_high = LOCK_ACTIVE_HIGH ? open : !open;
    if (level_high) {
        LOCK_PORT |= (1 << LOCK_PIN);
    } else {
        LOCK_PORT &= ~(1 << LOCK_PIN);
    }
}

static void unlock_door(void) {
    lcd_clear();
    lcd_print_string("Access Granted!");
    lcd_set_cursor(0, 1);
    lcd_print_string("Door Unlocked");
    lock_set(1);
    wait_ms(UNLOCK_TIME_MS);
    lock_set(0);
}

// --- Display helpers ---

// _delay_ms() needs a constant, so this waits in 10 ms steps
static void wait_ms(uint16_t ms) {
    while (ms >= 10) {
        _delay_ms(10);
        ms -= 10;
    }
}

static void show_message(const char *line1, const char *line2, uint16_t ms) {
    lcd_clear();
    lcd_print_string(line1);
    lcd_set_cursor(0, 1);
    lcd_print_string(line2);
    wait_ms(ms);
}

static void show_idle_screen(void) {
    char buffer[17];
    lcd_clear();
    lcd_print_string("Scan Your Card");
    sprintf(buffer, "Saved: %d/%d", eeprom_read_byte(EE_CARD_COUNT), MAX_CARDS);
    lcd_set_cursor(0, 1);
    lcd_print_string(buffer);
}

// --- PIN entry ---

/**
 * @brief Shows a prompt and reads PIN_LENGTH digits, shown as '*'.
 * @param pin Receives the digits plus a null terminator (PIN_LENGTH + 1 bytes).
 * @return 1 when a full PIN was confirmed with '#', 0 on cancel ('D') or timeout.
 */
static uint8_t read_pin(const char *prompt, char *pin) {
    uint8_t len = 0;
    uint16_t idle_ms = 0;

    lcd_clear();
    lcd_print_string(prompt);
    lcd_set_cursor(0, 1);

    while (idle_ms < PIN_TIMEOUT_MS) {
        char key = get_key();
        if (!key) {
            _delay_ms(10);
            idle_ms += 10;
            continue;
        }
        idle_ms = 0;

        if (key >= '0' && key <= '9') {
            if (len < PIN_LENGTH) {
                pin[len++] = key;
                lcd_print_string("*");
            }
        } else if (key == '*') {
            len = 0;
            lcd_set_cursor(0, 1);
            lcd_print_string("                ");
            lcd_set_cursor(0, 1);
        } else if (key == '#') {
            if (len == PIN_LENGTH) {
                pin[len] = '\0';
                return 1;
            }
        } else if (key == 'D') {
            return 0;
        }
    }
    return 0;
}

/**
 * @brief Asks for the PIN and checks it against the one saved in EEPROM.
 *        After MAX_WRONG_PINS wrong tries in a row, blocks for LOCKOUT_SECONDS.
 * @return 1 if the PIN was correct.
 */
static uint8_t verify_pin(const char *prompt) {
    char entered[PIN_LENGTH + 1];
    char saved[PIN_LENGTH];

    if (!read_pin(prompt, entered)) {
        show_message("Cancelled", "", 1000);
        return 0;
    }

    eeprom_read_block(saved, EE_PIN_ADDR, PIN_LENGTH);
    if (memcmp(entered, saved, PIN_LENGTH) == 0) {
        wrong_pin_count = 0;
        return 1;
    }

    wrong_pin_count++;
    if (wrong_pin_count < MAX_WRONG_PINS) {
        show_message("Wrong PIN", "", 1500);
        return 0;
    }

    // Too many wrong tries: lock everything for a while
    char buffer[17];
    for (uint8_t s = LOCKOUT_SECONDS; s > 0; s--) {
        sprintf(buffer, "Wait %2d sec", s);
        show_message("Too Many Tries", buffer, 1000);
    }
    wrong_pin_count = 0;
    return 0;
}

// --- Actions ---

// Waits for a card when adding one. Returns 1 if a card was read, 0 on timeout or 'D'.
static uint8_t wait_for_card(uint8_t *uid) {
    for (uint16_t t = 0; t < CARD_TIMEOUT_MS; t += 100) {
        if (MFRC522_IsCardPresent() && MFRC522_ReadCardSerial(uid)) {
            return 1;
        }
        if (get_key() == 'D') {
            return 0;
        }
        _delay_ms(100);
    }
    return 0;
}

static void handle_card(uint8_t *uid) {
    if (!is_uid_saved(uid)) {
        show_message("Access Denied", "Unknown Card", 1500);
        return;
    }
    if (verify_pin("Enter PIN:")) {
        unlock_door();
    }
}

static void add_card(void) {
    uint8_t uid[5];

    if (!verify_pin("Admin PIN:")) {
        return;
    }

    lcd_clear();
    lcd_print_string("Scan New Card");
    lcd_set_cursor(0, 1);
    lcd_print_string("D = Cancel");

    if (wait_for_card(uid)) {
        save_uid_to_eeprom(uid);
    } else {
        show_message("Cancelled", "", 1000);
    }
}

static void change_pin(void) {
    char new_pin[PIN_LENGTH + 1];
    char confirm[PIN_LENGTH + 1];

    if (!verify_pin("Current PIN:")) {
        return;
    }
    if (!read_pin("New PIN:", new_pin) || !read_pin("Confirm PIN:", confirm)) {
        show_message("Cancelled", "", 1000);
        return;
    }
    if (memcmp(new_pin, confirm, PIN_LENGTH) != 0) {
        show_message("PINs Don't Match", "Not Changed", 1500);
        return;
    }

    eeprom_update_block(new_pin, EE_PIN_ADDR, PIN_LENGTH);
    show_message("PIN Changed!", "", 1500);
}

static void clear_cards(void) {
    if (!verify_pin("Admin PIN:")) {
        return;
    }

    lcd_clear();
    lcd_print_string("Delete all cards");
    lcd_set_cursor(0, 1);
    lcd_print_string("#=Yes  D=No");

    for (uint16_t t = 0; t < PIN_TIMEOUT_MS; t += 10) {
        char key = get_key();
        if (key == '#') {
            eeprom_write_byte(EE_CARD_COUNT, 0);
            show_message("All Cards", "Deleted", 1500);
            return;
        }
        if (key == 'D') {
            break;
        }
        _delay_ms(10);
    }
    show_message("Cancelled", "", 1000);
}

// --- EEPROM Helper Functions ---

/**
 * @brief On first run (blank EEPROM), resets the card count and sets the default PIN.
 */
static void initialize_eeprom(void) {
    uint8_t setup_needed = 0;
    char pin[PIN_LENGTH];

    if (eeprom_read_byte(EE_CARD_COUNT) > MAX_CARDS) { // 0xFF or other garbage
        eeprom_write_byte(EE_CARD_COUNT, 0);
        setup_needed = 1;
    }

    eeprom_read_block(pin, EE_PIN_ADDR, PIN_LENGTH);
    for (uint8_t i = 0; i < PIN_LENGTH; i++) {
        if (pin[i] < '0' || pin[i] > '9') {
            eeprom_write_block(DEFAULT_PIN, EE_PIN_ADDR, PIN_LENGTH);
            setup_needed = 1;
            break;
        }
    }

    if (setup_needed) {
        show_message("First Run Setup", "PIN: " DEFAULT_PIN, 2000);
    }
}

static uint8_t is_uid_saved(uint8_t *uid_to_check) {
    uint8_t saved_card_count = eeprom_read_byte(EE_CARD_COUNT);
    uint8_t stored_uid[4];

    for (uint8_t i = 0; i < saved_card_count; i++) {
        eeprom_read_block(stored_uid, (const void *)(EE_CARDS_START + (i * 4)), 4);
        if (memcmp(uid_to_check, stored_uid, 4) == 0) {
            return 1; // Match found
        }
    }
    return 0; // No match
}

static void save_uid_to_eeprom(uint8_t *uid) {
    if (is_uid_saved(uid)) {
        show_message("Card Already", "Saved", 1500);
        return;
    }

    uint8_t saved_card_count = eeprom_read_byte(EE_CARD_COUNT);

    if (saved_card_count < MAX_CARDS) {
        uint16_t new_address = EE_CARDS_START + (saved_card_count * 4);
        eeprom_write_block(uid, (void *)new_address, 4);
        eeprom_write_byte(EE_CARD_COUNT, saved_card_count + 1);
        show_message("Card Saved!", "", 1500);
    } else {
        show_message("Memory Full!", "", 1500);
    }
}
