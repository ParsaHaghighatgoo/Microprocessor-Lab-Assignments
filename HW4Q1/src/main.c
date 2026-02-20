#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include "LCD.h"

#define RED_LED     PD3
#define GREEN_LED   PD4
#define BUZZER      PD5

volatile uint8_t overheat = 0;

void setup_all();
void read_and_display();
uint16_t read_adc(uint8_t channel);

void setup_all() {
    // ADC input pins
    DDRA &= ~((1 << PA0) | (1 << PA1));

    // Comparator pins AIN0 = PB2, AIN1 = PB3
    DDRB &= ~((1 << PB2) | (1 << PB3));

    // LCD data = PORTC, ctrl = PD0-2, outputs
    DDRC = 0xFF;
    DDRD |= (1 << RED_LED) | (1 << GREEN_LED) | (1 << BUZZER) | (1 << PD0) | (1 << PD1) | (1 << PD2);

    // ADC setup
    ADMUX = 0x00; // AREF, channel 0
    ADCSRA = (1 << ADEN) | (1 << ADPS2); // ADC enable, prescaler /16

    // Analog comparator interrupt
    ACSR = (1 << ACIE);     // Enable analog comparator interrupt
    SFIOR &= ~(1 << ACME);  // Use AIN0 & AIN1

    // Timer0 CTC mode for buzzer toggling (~1kHz toggle rate)
    TCCR0 = (1 << WGM01) | (1 << CS01) | (1 << CS00); // CTC, prescaler 64
    OCR0 = 124;              // 8MHz / 64 / (124+1) ≈ 1kHz
    TIMSK |= (1 << OCIE0);   // Compare match interrupt enable

    sei(); // Global interrupt enable

    LCD_initialize();
    LCD_clear();
}

uint16_t read_adc(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

void read_and_display() {
    uint16_t t_server = read_adc(0);   // ADC0 = PA0
    uint16_t t_control = read_adc(1);  // ADC1 = PA1

    int ts = (int)(t_server * 5.0 / 1023 / 0.01);
    int tc = (int)(t_control * 5.0 / 1023 / 0.01);

    char buffer[16];

    LCD_goto_xy(0, 0);
    sprintf(buffer, "Server:  %3dC", ts);
    LCD_print_text(buffer);

    LCD_goto_xy(1, 0);
    sprintf(buffer, "Control: %3dC", tc);
    LCD_print_text(buffer);

    if (ts > tc) {
        PORTD |= (1 << RED_LED);
        PORTD &= ~(1 << GREEN_LED);
        overheat = 1;  // Enable buzzer toggling
    } else {
        PORTD |= (1 << GREEN_LED);
        PORTD &= ~(1 << RED_LED);
        overheat = 0;
        PORTD &= ~(1 << BUZZER); // Make sure buzzer is OFF
    }
}

ISR(ANA_COMP_vect) {
    _delay_ms(200);
    read_and_display();
}

ISR(TIMER0_COMP_vect) {
    if (overheat) {
        PORTD ^= (1 << BUZZER); // Toggle buzzer pin
    }
}

int main(void) {
    setup_all();
    read_and_display();  // Initial display

    while (1) {
        // Idle — Interrupts handle everything
    }
}
