MCU := atmega32a
F_CPU := 16000000UL
TARGET := firmware

CC := avr-gcc
OBJCOPY := avr-objcopy

CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=c11 -Wall -Wextra -Iinclude
LDFLAGS := -mmcu=$(MCU)

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all clean flash model eeprom-image

all: $(TARGET).hex

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

model:
	python3 tools/export_model_params.py

eeprom-image:
	python3 tools/export_eeprom_model.py

flash: all
	@echo "Example (USBasp): avrdude -p m32 -c usbasp -U flash:w:$(TARGET).hex"

clean:
	rm -f src/*.o $(TARGET).elf $(TARGET).hex
