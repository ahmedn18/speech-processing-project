#ifndef EEPROM24C512_H
#define EEPROM24C512_H

#include <stdint.h>

void eeprom24c512_init(void);
uint8_t eeprom24c512_probe(void);
uint8_t eeprom24c512_read(uint16_t address, uint8_t *data, uint16_t length);

#endif
