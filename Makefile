# Makefile for AVR ATmega328P

# Compiler and flags
CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS = -Os -Wall -mmcu=atmega328p -DF_CPU=8000000UL

# Source files
SOURCES = main.c i2c_lcd.c mfrc522.c keypad.c

# Object files (derived from source files)
OBJECTS = $(SOURCES:.c=.o)

# Target executable
TARGET = main.elf
HEX_TARGET = main.hex

# Default rule
all: $(HEX_TARGET)

# Rule to link object files into the final ELF executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Rule to create the HEX file from the ELF file
$(HEX_TARGET): $(TARGET)
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET) $(HEX_TARGET)

# Rule to compile C source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to upload the code using avrdude
upload: $(HEX_TARGET)
	avrdude -c usbasp -p m328p -U flash:w:$(HEX_TARGET)

# Rule to clean up generated files
clean:
	rm -f $(OBJECTS) $(TARGET) $(HEX_TARGET)