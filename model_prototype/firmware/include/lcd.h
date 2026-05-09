#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void LCD_Init(void);
void LCD_Command(unsigned char cmd);
void LCD_Char(unsigned char data);
void LCD_String(char *str);
void LCD_String_xy(char row, char pos, char *str);
void LCD_Clear(void);
void LCD_Create_Char(unsigned char address, unsigned char pattern[]);
void LCD_Gotoxy(char row, char pos);
unsigned char LCD_Read_Char(unsigned char address);

#endif
