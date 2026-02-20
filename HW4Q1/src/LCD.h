#ifndef LCD_H
#define LCD_H

void LCD_initialize(void);
void LCD_send_command(unsigned char cmd);
void LCD_send_data(unsigned char data);
void LCD_clear(void);
void LCD_goto_xy(unsigned char row, unsigned char col);
void LCD_print_text(const char* str);

#endif
