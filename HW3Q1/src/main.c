#include <avr/io.h>
#include <util/delay.h>
#include <LCD.h>
#include <avr/interrupt.h>
#include <stdio.h>

double pwm_percentage = 0;
int count = 0;

void update_display(int value) {
    char text[10];
    LCD_cmd(0x01); // Clear display
    _delay_ms(50);
    sprintf(text, "%d", value);
    for (int j = 0; text[j] != '\0'; j++) {
        LCD_write(text[j]);
    }
    _delay_ms(10);
}

void setup_pwm_timer() {
    DDRB |= (1 << PB3); // OC0 pin as output

    // Set Fast PWM mode
    TCCR0 |= (1 << WGM01) | (1 << WGM00);

    // Non-inverting mode
    TCCR0 |= (1 << COM01);

    // Prescaler set to clk/8
    TCCR0 |= (1 << CS01);

    // Initial duty cycle
    OCR0 = (pwm_percentage / 100) * 255;
}

void setup_interrupts() {
    sei(); // Enable global interrupts
    GICR |= (1 << INT1) | (1 << INT2);
    MCUCR = 0x0C;     // INT1 falling edge
    MCUCSR = 0x40;    // INT2 rising edge
}

void setup_lcd() {
    DDRA = 0xFF;
    DDRD = 0x07;
    init_LCD();
    LCD_cmd(0x0F); // Cursor on, blinking
    _delay_ms(150);
    
    const char message[] = "Hello";
    for (int i = 0; message[i] != '\0'; i++) {
        LCD_write(message[i]);
        _delay_ms(10);
    }

    _delay_ms(150);
    LCD_cmd(0x01); // Clear screen
    _delay_ms(50);

    update_display(count);
}

int main(void) {
    setup_pwm_timer();
    setup_interrupts();
    setup_lcd();

    while (1) {
        // Main loop does nothing, work is interrupt-driven
    }

    return 0;
}

ISR(INT1_vect) {
    if (count < 9) {
        count++;
        pwm_percentage += 10;
        OCR0 = (pwm_percentage / 100) * 255;
        update_display(count);
    }
}

ISR(INT2_vect) {
    if (count > 0) {
        count--;
        pwm_percentage -= 10;
        OCR0 = (pwm_percentage / 100) * 255;
        update_display(count);
    }
}
