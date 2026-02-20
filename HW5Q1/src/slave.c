#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define SWITCH_MODE 0xA0
#define HIGH_TEMP 0xB0
#define NORMAL_TEMP 0xC0

#define F_CPU 8000000UL
#define BAUDRATE 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUDRATE)) - 1)

volatile uint8_t current_mode = 0;  // 0: NORMAL, 1: PERFORMANCE

void USART_setup(void) {
    uint16_t ubrr = UBRR_VAL;

    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t)(ubrr);

    UCSRB = (1 << RXEN) | (1 << TXEN);  // Enable receiver and transmitter
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);  // 8-bit data format
}

void USART_send(uint8_t byte) {
    while (!(UCSRA & (1 << UDRE)));  // Wait until buffer is empty
    UDR = byte;
}

uint8_t USART_receive(void) {
    while (!(UCSRA & (1 << RXC)));  // Wait until data is received
    return UDR;
}

void ADC_initialize(void) {
    ADMUX = (1 << REFS0);  // AVCC as voltage reference
    ADCSRA = (1 << ADEN) | (7 << ADPS0);  // Enable ADC, set prescaler
}

uint16_t get_temperature(void) {
    ADMUX &= 0xF0;  // Use ADC0
    ADCSRA |= (1 << ADSC);  // Start conversion
    while (ADCSRA & (1 << ADSC));  // Wait until done

    uint16_t value = ADC;
    return ((uint32_t)value * 488) / 1000;  // Convert ADC value to Celsius approx.
}

void PWM_configure(void) {
    // Fan 1 (OC0 on PB3)
    DDRB |= (1 << PB3);
    TCCR0 = (1 << WGM00) | (1 << WGM01)
          | (1 << COM01)
          | (1 << CS01);

    // Fan 2 (OC1A on PD5)
    DDRD |= (1 << PD5);
    TCCR1A = (1 << WGM10) | (1 << COM1A1);
    TCCR1B = (1 << WGM12) | (1 << CS11);

    // Fan 3 (OC2 on PD7)
    DDRD |= (1 << PD7);
    TCCR2 = (1 << WGM20) | (1 << WGM21)
          | (1 << COM21)
          | (1 << CS21);
}

uint8_t limit(uint8_t val) {
    return (val > 100) ? 100 : val;
}

void apply_fan_speeds(uint8_t s1, uint8_t s2, uint8_t s3) {
    OCR0 = (limit(s1) * 255) / 100;
    OCR1A = (limit(s2) * 255) / 100;
    OCR2 = (limit(s3) * 255) / 100;
}

int main(void) {
    USART_setup();
    ADC_initialize();
    PWM_configure();

    while (1) {
        if (UCSRA & (1 << RXC)) {
            if (UDR == SWITCH_MODE) {
                current_mode ^= 1;  // Toggle between modes
            }
        }

        uint8_t temp = get_temperature();

        if (current_mode == 0) {
            if (temp > 100) {
                apply_fan_speeds(100, 100, 0);
                USART_send(HIGH_TEMP);
            } else if (temp >= 50) {
                apply_fan_speeds(temp, temp, 0);
                USART_send(NORMAL_TEMP);
            } else if (temp >= 10) {
                apply_fan_speeds(temp, 0, 0);
                USART_send(NORMAL_TEMP);
            } else {
                apply_fan_speeds(0, 0, 0);
                USART_send(NORMAL_TEMP);
            }
        } else {
            apply_fan_speeds(temp, temp, temp);
            USART_send(NORMAL_TEMP);
        }

        _delay_ms(300);
    }

    return 0;
}
