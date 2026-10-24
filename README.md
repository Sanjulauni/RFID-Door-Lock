# RFID Door Lock

An RFID card + PIN door lock for the **ATmega328P** microcontroller, written in plain C (no Arduino libraries).

To open the door you scan a saved RFID card **and** type the correct 4-digit PIN on a keypad. A 16x2 LCD shows what's happening, and a relay or solenoid driver on one output pin unlocks the door for 3 seconds.

## Features

- **Two-factor access:** a saved card plus the PIN
- **Up to 10 cards**, stored in EEPROM so they survive power loss
- **Keypad menu** to add cards, change the PIN, or delete all cards, each protected by the PIN
- **Brute-force protection:** 3 wrong PINs in a row lock the system for 30 seconds
- **PIN shown as `*`** while typing, with a 10-second timeout
- **Unknown cards are refused.** New cards can only be added through the menu.

## Hardware

| Part | Notes |
|---|---|
| ATmega328P | Runs at 8 MHz (on its own or on an Arduino Uno-style board) |
| MFRC522 RFID reader | 13.56 MHz module, SPI interface, **3.3 V power** |
| 16x2 character LCD with I2C backpack | PCF8574 backpack at address `0x27` |
| 4x4 matrix keypad | Keys `0-9`, `A-D`, `*`, `#` |
| Relay module or MOSFET + solenoid lock | Driven from one pin. Use a flyback diode across the solenoid |
| USBasp programmer | For flashing the chip |

## Wiring

| Signal | ATmega328P pin | Arduino Uno pin |
|---|---|---|
| MFRC522 SDA (SS) | PB2 | D10 |
| MFRC522 SCK | PB5 | D13 |
| MFRC522 MOSI | PB3 | D11 |
| MFRC522 MISO | PB4 | D12 |
| MFRC522 RST | PB1 | D9 |
| LCD SDA | PC4 | A4 |
| LCD SCL | PC5 | A5 |
| Keypad rows 1-4 | PD0-PD3 | D0-D3 |
| Keypad columns 1-4 | PD4-PD7 | D4-D7 |
| **Lock output** | **PB0** | **D8** |

> The keypad uses PD0/PD1, which are also the serial RX/TX pins, so the serial port and the Arduino USB bootloader can't be used. Flash the chip with the USBasp instead.

## How to use it

When idle, the LCD shows `Scan Your Card` and how many cards are saved.

### Opening the door
1. Scan a saved card.
2. Type the 4-digit PIN and press `#`.
3. The LCD shows `Access Granted!` and the lock opens for 3 seconds.

An unknown card shows `Access Denied`.

### Keypad keys

| Key | Action |
|---|---|
| `0`-`9` | PIN digits |
| `#` | Confirm |
| `*` | Clear what you typed |
| `D` | Cancel |
| `A` | Add a new card (asks for the PIN, then scan the card) |
| `B` | Change the PIN (current PIN, new PIN, then confirm) |
| `C` | Delete all saved cards (asks for the PIN, then `#` to confirm) |

### First run
On a blank chip, the LCD shows `First Run Setup`, the card list is emptied and the PIN is set to **`1234`**. Change it with `B` straight away. To add your first card, press `A` and use the default PIN.

## Configuration

These settings are at the top of [`main.c`](main.c):

| Setting | Default | Meaning |
|---|---|---|
| `LOCK_ACTIVE_HIGH` | `1` | Set to `0` for relay modules that switch on a LOW input |
| `UNLOCK_TIME_MS` | `3000` | How long the door stays unlocked |
| `DEFAULT_PIN` | `"1234"` | PIN set on first run |
| `MAX_CARDS` | `10` | Number of cards that can be saved |
| `MAX_WRONG_PINS` | `3` | Wrong PINs allowed before a lockout |
| `LOCKOUT_SECONDS` | `30` | Lockout length |
| `PIN_TIMEOUT_MS` | `10000` | PIN entry gives up after this long with no key pressed |
| `LCD_ADDRESS` | `0x27` | I2C address of the LCD backpack (some use `0x3F`) |

## EEPROM layout

| Address | Contents |
|---|---|
| 0 | Number of saved cards (0-10) |
| 1-40 | Card IDs, 4 bytes each |
| 48-51 | PIN as ASCII digits |

## Building and flashing

You need `avr-gcc`, `avr-libc`, `make` and `avrdude`. The Arduino IDE ships `avr-gcc` and `avrdude`, or you can install a standalone AVR toolchain such as WinAVR.

```bash
make
```

This builds `main.hex`. To flash it with a USBasp:

```bash
make upload
```

To remove the build files:

```bash
make clean
```

The code expects an **8 MHz** clock (`F_CPU` is set in the [`Makefile`](Makefile)). If you use the chip's internal 8 MHz oscillator, clear the `CKDIV8` fuse (low fuse `0xE2`). Otherwise the chip runs at 1 MHz and all timing will be 8x too slow.

## Project files

| File | What it does |
|---|---|
| [`main.c`](main.c) | Main program: card check, PIN entry, keypad menu, lock control and EEPROM storage |
| [`mfrc522.c`](mfrc522.c) / [`mfrc522.h`](mfrc522.h) | MFRC522 RFID reader driver over SPI (detects a card and reads its 4-byte ID) |
| [`i2c_lcd.c`](i2c_lcd.c) / [`i2c_lcd.h`](i2c_lcd.h) | I2C (TWI) driver for the 16x2 LCD, at 100 kHz |
| [`keypad.c`](keypad.c) / [`keypad.h`](keypad.h) | 4x4 keypad scanner with simple debounce |
| [`Makefile`](Makefile) | Build, upload and clean rules |

## Limitations

- Only the first 4 bytes of a card's ID are used, which suits common MIFARE Classic cards. 7-byte-ID cards aren't fully supported.
- Card IDs can be cloned, which is why a PIN is also required.
- The PIN and card list are stored unencrypted in EEPROM.
- Everything shares one PIN. There are no separate admin and user PINs.
