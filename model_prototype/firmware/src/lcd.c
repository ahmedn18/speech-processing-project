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

void LCD_Init(void) {
  LCD_RS_DDR |= (1U << LCD_RS_PIN);
  LCD_RW_DDR |= (1U << LCD_RW_PIN);
  LCD_EN_DDR |= (1U << LCD_EN_PIN);
  LCD_DATA_DDR |= (uint8_t)(0x0FU << LCD_DATA_SHIFT);

  LCD_RS_PORT &= (uint8_t)~(1U << LCD_RS_PIN);
  LCD_RW_PORT &= (uint8_t)~(1U << LCD_RW_PIN);
  LCD_EN_PORT &= (uint8_t)~(1U << LCD_EN_PIN);
  LCD_DATA_PORT &= (uint8_t)~(0x0FU << LCD_DATA_SHIFT);

  _delay_ms(40);
  LCD_RS_PORT &= (uint8_t)~(1U << LCD_RS_PIN);

  lcd_write_nibble(0x03U);
  _delay_ms(5);
  lcd_write_nibble(0x03U);
  _delay_us(150);
  lcd_write_nibble(0x03U);
  lcd_write_nibble(0x02U);

  lcd_command(LCD_FUNCTION_SET_4BIT);
  lcd_command(LCD_DISPLAY_ON);
  lcd_command(LCD_ENTRY_MODE_SET);
  LCD_Clear();
}

void LCD_Command(unsigned char cmd) {
  lcd_command(cmd);
  if (cmd == LCD_CLEAR_DISPLAY || cmd == LCD_RETURN_HOME) {
    _delay_ms(2);
  }
}

void LCD_Char(unsigned char data) {
  lcd_send(data, 1U);
}

void LCD_String(char *str) {
  while (*str != '\0') {
    LCD_Char((unsigned char)*str);
    str++;
  }
}

void LCD_String_xy(char row, char pos, char *str) {
  LCD_Gotoxy(row, pos);
  LCD_String(str);
}

void LCD_Clear(void) {
  LCD_Command(LCD_CLEAR_DISPLAY);
}

void LCD_Create_Char(unsigned char address, unsigned char pattern[]) {
  address &= 0x07U;

  LCD_Command((unsigned char)(LCD_SET_CGRAM_ADDR | (address << 3)));
  for (uint8_t i = 0; i < 8; i++) {
    LCD_Char(pattern[i]);
  }

  LCD_Command(LCD_SET_DDRAM_ADDR);
}

void LCD_Gotoxy(char row, char pos) {
  unsigned char address;

  if (row == 0) {
    address = (unsigned char)(LCD_ROW0_ADDR + (unsigned char)pos);
  } else {
    address = (unsigned char)(LCD_ROW1_ADDR + (unsigned char)pos);
  }

  LCD_Command((unsigned char)(LCD_SET_DDRAM_ADDR | address));
}

unsigned char LCD_Read_Char(unsigned char address) {
  unsigned char data = 0;

  LCD_Command((unsigned char)(LCD_SET_DDRAM_ADDR | address));

  LCD_DATA_DDR &= (uint8_t)~(0x0FU << LCD_DATA_SHIFT);
  LCD_DATA_PORT |= (uint8_t)(0x0FU << LCD_DATA_SHIFT);

  LCD_RS_PORT |= (1U << LCD_RS_PIN);
  LCD_RW_PORT |= (1U << LCD_RW_PIN);

  LCD_EN_PORT |= (1U << LCD_EN_PIN);
  _delay_us(1);
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 3U))) data |= 0x80U;
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 2U))) data |= 0x40U;
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 1U))) data |= 0x20U;
  if (LCD_DATA_PIN & (1U << LCD_DATA_SHIFT)) data |= 0x10U;
  LCD_EN_PORT &= (uint8_t)~(1U << LCD_EN_PIN);
  _delay_us(1);

  LCD_EN_PORT |= (1U << LCD_EN_PIN);
  _delay_us(1);
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 3U))) data |= 0x08U;
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 2U))) data |= 0x04U;
  if (LCD_DATA_PIN & (1U << (LCD_DATA_SHIFT + 1U))) data |= 0x02U;
  if (LCD_DATA_PIN & (1U << LCD_DATA_SHIFT)) data |= 0x01U;
  LCD_EN_PORT &= (uint8_t)~(1U << LCD_EN_PIN);

  LCD_RW_PORT &= (uint8_t)~(1U << LCD_RW_PIN);
  LCD_DATA_DDR |= (uint8_t)(0x0FU << LCD_DATA_SHIFT);

  return data;
}
