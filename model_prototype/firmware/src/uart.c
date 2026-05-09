#include "uart.h"

#include <avr/io.h>

#include "config.h"

void uart_init(uint32_t baudrate) {
    uint16_t ubrr = (uint16_t) ((F_CPU / (16UL * baudrate)) - 1UL);
    UBRRH = (uint8_t) (ubrr >> 8);
    UBRRL = (uint8_t) ubrr;
    UCSRB = (1U << RXEN) | (1U << TXEN);
    UCSRC = (1U << URSEL) | (1U << UCSZ1) | (1U << UCSZ0);
}

void uart_write_char(char c) {
    while ((UCSRA & (1U << UDRE)) == 0U) {
    }
    UDR = (uint8_t) c;
}

char uart_read_char(void) {
    while ((UCSRA & (1U << RXC)) == 0U) {
    }
    return (char) UDR;
}

void uart_write_text(const char *text) {
    while (*text != '\0') {
        uart_write_char(*text);
        text++;
    }
}
