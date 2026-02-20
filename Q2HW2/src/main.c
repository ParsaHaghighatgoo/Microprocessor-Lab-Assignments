#include <avr/io.h>
#include <util/delay.h>



int main(){

    DDRA = (0<<DDA7) | (0<<DDA6) | (0<<DDA5) | (0<<DDA4) | (0<<DDA3) | (0<<DDA2) | (0<<DDA1) | (1<<DDA0);
    DDRB = (0<<DDB7) | (0<<DDB6) | (0<<DDB5) | (0<<DDB4) | (0<<DDB3) | (0<<DDB2) | (0<<DDB1) | (0<<DDB0);
    PORTB = (0<<PORTB7) | (0<<PORTB6) | (0<<PORTB5) | (0<<PORTB4) | (0<<PORTB3) | (0<<PORTB2) | (1<<PORTB1) | (1<<PORTB0);

    while (1)
    {
        _delay_ms(5);

        if(((PINB >> PINB0) & 1) == 0){
            PORTA |= (1 << PORTA0);
        }

        if(((PINB >> PINB1) & 1) == 0){
            PORTA &= (0 << PORTA0);
        }
    }
    
}