#include <avr/io.h>
#include <util/delay.h>
#include "LCD.h"

#define LCD_DATA_PORT PORTC
#define LCD_CTRL_PORT PORTD

#define LCD_RS PD0
#define LCD_RW PD1
#define LCD_EN PD2

void LCD_send_command(unsigned char cmd)
{
    LCD_DATA_PORT = cmd;
    LCD_CTRL_PORT &= ~(1 << LCD_RS); // RS = 0
    LCD_CTRL_PORT &= ~(1 << LCD_RW); // RW = 0
    LCD_CTRL_PORT |= (1 << LCD_EN);
    _delay_ms(1);
    LCD_CTRL_PORT &= ~(1 << LCD_EN);
    _delay_ms(2);
}

void LCD_send_data(unsigned char data)
{
    LCD_DATA_PORT = data;
    LCD_CTRL_PORT |= (1 << LCD_RS);  // RS = 1
    LCD_CTRL_PORT &= ~(1 << LCD_RW); // RW = 0
    LCD_CTRL_PORT |= (1 << LCD_EN);
    _delay_ms(1);
    LCD_CTRL_PORT &= ~(1 << LCD_EN);
    _delay_ms(2);
}

void LCD_initialize(void)
{
    _delay_ms(20);
    LCD_send_command(0x38); // 8-bit, 2 line, 5x8
    LCD_send_command(0x0C); // Display ON, cursor OFF
    LCD_send_command(0x06); // Auto increment cursor
    LCD_send_command(0x01); // Clear screen
    _delay_ms(2);
}

void LCD_clear(void)
{
    LCD_send_command(0x01);
    _delay_ms(2);
}

void LCD_goto_xy(unsigned char row, unsigned char col)
{
    unsigned char pos = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_send_command(pos);
}

void LCD_print_text(const char* str)
{
    while (*str)
        LCD_send_data(*str++);
}
