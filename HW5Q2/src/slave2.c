// --- Include AVR headers ---
#include <avr/io.h>
#include <avr/interrupt.h>

// --- SPI pin definitions for slave ---
#define SPI_SS    PB4
#define SPI_MOSI  PB5
#define SPI_MISO  PB6
#define SPI_SCK   PB7

// --- SPI control flags ---
#define SPI_ENABLE_FLAG       (1 << SPE)
#define SPI_INTERRUPT_FLAG    (1 << SPIE)

// --- Response character for Slave 2 ---
volatile char response_char_slave2 = 'C';

// --- Initialize SPI for Slave 2 ---
void setup_slave2_spi(void) {
    // Configure SS, MOSI, and SCK pins as inputs
    DDRB &= ~((1 << SPI_SS) | (1 << SPI_MOSI) | (1 << SPI_SCK));
    // Set MISO as output
    DDRB |= (1 << SPI_MISO);

    // Enable SPI peripheral and its interrupt
    SPCR = SPI_ENABLE_FLAG | SPI_INTERRUPT_FLAG;

    // Enable global interrupts
    sei();
}

// --- SPI transfer complete ISR for Slave 2 ---
ISR(SPI_STC_vect) {
    char received_cmd = SPDR;

    // If received '2', respond with 'B', else respond with 'C'
    if (received_cmd == '2') {
        response_char_slave2 = 'B';
    } else {
        response_char_slave2 = 'C';
    }

    SPDR = response_char_slave2;
}

// --- Main program ---
int main(void) {
    setup_slave2_spi();

    while (1) {
        // Idle loop; SPI communication handled in ISR
    }
}
