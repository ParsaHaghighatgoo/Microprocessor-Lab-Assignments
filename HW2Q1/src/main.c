#include <avr/io.h>
#include <util/delay.h>



int main(){

    DDRB = (0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (0<<DDB1) | (1<<DDB0);

    PORTB |= (1 << PB0);
    _delay_ms(1000);


    while (1)
    {
        PORTB ^= (1 << PB0);
        _delay_ms(100);
    }
    
}