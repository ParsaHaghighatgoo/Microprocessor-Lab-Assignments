#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <LCD.h>

#define SWITCH_MODE_CMD 0xA0
#define TEMP_TOO_HIGH 0xB0
#define TEMP_OKAY 0xC0

#define F_CPU 8000000UL
#define BAUDRATE 9600
#define BAUD_CALC ((F_CPU / (16UL * BAUDRATE)) - 1)

#define LCD_CTRL_RS PB0
#define LCD_CTRL_RW PB1
#define LCD_CTRL_EN PB2

#define CONFIG_INPUT_PULLUP(dir_reg, port_reg, pin_no) \
    do { \
        (dir_reg) &= ~(1 << (pin_no)); \
        (port_reg) |= (1 << (pin_no)); \
    } while (0)

#define INT1_TRIGGER_ON_FALLING() \
    MCUCR = (MCUCR & ~(1 << ISC10)) | (1 << ISC11); \
    GICR |= (1 << INT1)

volatile uint8_t mode = 0;  // 0 = Normal Mode, 1 = Performance Mode

void uart_initialize(void) {
    uint16_t ubrr = BAUD_CALC;

    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t)(ubrr);

    UCSRB = (1 << RXEN) | (1 << TXEN);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void uart_send(uint8_t byte) {
    while (!(UCSRA & (1 << UDRE)));
    UDR = byte;
}

uint8_t uart_receive(void) {
    while (!(UCSRA & (1 << RXC)));
    return UDR;
}

void lcd_display_text(const char *msg) {
    while (*msg) {
        LCD_write(*msg++);
    }
}

void refresh_lcd_mode(void) {
    LCD_cmd(0x01);   // Clear screen
    LCD_cmd(0x80);   // Cursor to line 1
    if (mode == 0)
        lcd_display_text("NORMAL MODE");
    else
        lcd_display_text("PERFORMANCE MODE");
}

ISR(INT1_vect) {
    if (!(PIND & (1 << PD3))) {
        mode ^= 1;
        uart_send(SWITCH_MODE_CMD);
        refresh_lcd_mode();
    }
}

void display_mode_on_lcd(void) {
    LCD_cmd(0x01);
    LCD_cmd(0x80);
    lcd_display_text(mode == 0 ? "NORMAL MODE" : "PERFORMANCE MODE");
}

void display_warning(void) {
    LCD_cmd(0x01);
    LCD_cmd(0x80);
    lcd_display_text("THERMAL THROTTLING");
}

int main() {
    _delay_ms(500); // Startup delay

    DDRA = 0xFF;  // All LCD data pins as output
    DDRB |= (1 << LCD_CTRL_RS) | (1 << LCD_CTRL_RW) | (1 << LCD_CTRL_EN);  // LCD control pins

    init_LCD();
    lcd_display_text("NORMAL MODE");

    CONFIG_INPUT_PULLUP(DDRD, PORTD, PD3);  // Button on PD3

    INT1_TRIGGER_ON_FALLING();
    sei();

    uart_initialize();

    while (1) {
        uint8_t received = uart_receive();

        if (received == TEMP_OKAY) {
            display_mode_on_lcd();
        } else if (received == TEMP_TOO_HIGH) {
            if (mode == 0)
                display_warning();
            else
                display_mode_on_lcd();
        }
    }
}
