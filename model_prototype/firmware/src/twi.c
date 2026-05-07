#include "twi.h"

#include <avr/io.h>
#include <util/twi.h>

void twi_init(uint32_t scl_hz) {
    uint32_t twbr_value = ((F_CPU / scl_hz) - 16UL) / 2UL;

    TWSR = 0x00U;
    TWBR = (uint8_t) twbr_value;
    TWCR = (1U << TWEN);
}

uint8_t twi_start(uint8_t address_rw) {
    TWCR = (1U << TWINT) | (1U << TWSTA) | (1U << TWEN);
    while ((TWCR & (1U << TWINT)) == 0U) {
    }

    if ((TW_STATUS != TW_START) && (TW_STATUS != TW_REP_START)) {
        return 0U;
    }

    TWDR = address_rw;
    TWCR = (1U << TWINT) | (1U << TWEN);
    while ((TWCR & (1U << TWINT)) == 0U) {
    }

    if ((TW_STATUS != TW_MT_SLA_ACK) && (TW_STATUS != TW_MR_SLA_ACK)) {
        return 0U;
    }

    return 1U;
}

void twi_stop(void) {
    TWCR = (1U << TWINT) | (1U << TWEN) | (1U << TWSTO);
    while (TWCR & (1U << TWSTO)) {
    }
}

uint8_t twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1U << TWINT) | (1U << TWEN);
    while ((TWCR & (1U << TWINT)) == 0U) {
    }

    return (TW_STATUS == TW_MT_DATA_ACK) ? 1U : 0U;
}

uint8_t twi_read_ack(uint8_t *data) {
    TWCR = (1U << TWINT) | (1U << TWEN) | (1U << TWEA);
    while ((TWCR & (1U << TWINT)) == 0U) {
    }
    if (TW_STATUS != TW_MR_DATA_ACK) {
        return 0U;
    }
    *data = TWDR;
    return 1U;
}

uint8_t twi_read_nack(uint8_t *data) {
    TWCR = (1U << TWINT) | (1U << TWEN);
    while ((TWCR & (1U << TWINT)) == 0U) {
    }
    if (TW_STATUS != TW_MR_DATA_NACK) {
        return 0U;
    }
    *data = TWDR;
    return 1U;
}
