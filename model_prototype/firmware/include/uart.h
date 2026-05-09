#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baudrate);
void uart_write_char(char c);
char uart_read_char(void);
void uart_write_text(const char *text);

#endif
