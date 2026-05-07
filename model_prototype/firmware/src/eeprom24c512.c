#include "eeprom24c512.h"

#include "config.h"
#include "twi.h"

void eeprom24c512_init(void) {
    twi_init(TWI_SCL_HZ);
}

uint8_t eeprom24c512_probe(void) {
    uint8_t ok = twi_start((uint8_t) ((EEPROM24C512_I2C_ADDR << 1) | 0U));
    twi_stop();
    return ok;
}

uint8_t eeprom24c512_read(uint16_t address, uint8_t *data, uint16_t length) {
    uint16_t i;

    if (length == 0U) {
        return 1U;
    }

    if (!twi_start((uint8_t) ((EEPROM24C512_I2C_ADDR << 1) | 0U))) {
        twi_stop();
        return 0U;
    }

    if (!twi_write((uint8_t) (address >> 8))) {
        twi_stop();
        return 0U;
    }
    if (!twi_write((uint8_t) (address & 0xFFU))) {
        twi_stop();
        return 0U;
    }

    if (!twi_start((uint8_t) ((EEPROM24C512_I2C_ADDR << 1) | 1U))) {
        twi_stop();
        return 0U;
    }

    for (i = 0; i < length; i++) {
        uint8_t ok = (i + 1U < length) ? twi_read_ack(&data[i]) : twi_read_nack(&data[i]);
        if (!ok) {
            twi_stop();
            return 0U;
        }
    }

    twi_stop();
    return 1U;
}
