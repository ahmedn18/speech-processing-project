#include "lcd.h"

#include <avr/io.h>
#include <util/delay.h>

#include "config.h"

static void lcd_pulse_enable(void) {
  LCD_EN_PORT |= (1U << LCD_EN_PIN);
  _delay_us(1);
  LCD_EN_PORT &= (uint8_t)~(1U << LCD_EN_PIN);
  _delay_us(80);
}

static void lcd_write_nibble(uint8_t nibble) {
  LCD_DATA_PORT &= (uint8_t)~(0x0FU << LCD_DATA_SHIFT);
  LCD_DATA_PORT |= (uint8_t)((nibble & 0x0FU) << LCD_DATA_SHIFT);
  lcd_pulse_enable();
}

static void lcd_send(uint8_t value, uint8_t rs) {
  if (rs) {
    LCD_RS_PORT |= (1U << LCD_RS_PIN);
  } else {
    LCD_RS_PORT &= (uint8_t)~(1U << LCD_RS_PIN);
  }

  lcd_write_nibble((uint8_t)(value >> 4));
  lcd_write_nibble(value);
}

static void lcd_command(uint8_t cmd) {
  lcd_send(cmd, 0U);
  _delay_ms(2);
}

void lcd_init(void) {

  LCD_RS_DDR |= (1U << LCD_RS_PIN);
  LCD_EN_DDR |= (1U << LCD_EN_PIN);
  LCD_DATA_DDR |= (uint8_t)(0x0FU << LCD_DATA_SHIFT);
  LCD_RS_PORT &= ~(1U << LCD_RS_PIN);
  LCD_EN_PORT &= ~(1U << LCD_EN_PIN);
  LCD_DATA_PORT &= ~(0x0F << LCD_DATA_SHIFT);

  _delay_ms(40);
  LCD_RS_PORT &= (uint8_t)~(1U << LCD_RS_PIN);

  lcd_write_nibble(0x03U);
  _delay_ms(5);
  lcd_write_nibble(0x03U);
  _delay_us(150);
  lcd_write_nibble(0x03U);
  lcd_write_nibble(0x02U);

  lcd_command(0x28U);
  lcd_command(0x0CU);
  lcd_command(0x06U);
  lcd_clear();
}

void lcd_clear(void) {
  lcd_command(0x01U);
  _delay_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
  uint8_t offset = (row == 0U) ? 0x00U : 0x40U;
  lcd_command((uint8_t)(0x80U | (offset + col)));
}

void lcd_print(const char *text) {
  while (*text != '\0') {
    lcd_send((uint8_t)*text, 1U);
    text++;
  }
}
