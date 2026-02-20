// --- Included Headers ---
#include <avr/io.h>
#include <avr/interrupt.h>

// --- SPI Pins for Slave Setup ---
#define SPI_SS    PB4
#define SPI_MOSI  PB5
#define SPI_MISO  PB6
#define SPI_SCK   PB7

// --- SPI Control Bits ---
#define SPI_ENABLE_BIT       (1 << SPE)
#define SPI_INT_ENABLE_BIT   (1 << SPIE)

// --- Default response character ---
volatile char response_char = 'C';

// --- Initialize SPI Slave 1 ---
void init_slave1_spi(void) {
    // Configure SS, MOSI, SCK as inputs
    DDRB &= ~((1 << SPI_SS) | (1 << SPI_MOSI) | (1 << SPI_SCK));

    // Configure MISO as output
    DDRB |= (1 << SPI_MISO);

    // Enable SPI and SPI interrupt
    SPCR = SPI_ENABLE_BIT | SPI_INT_ENABLE_BIT;

    // Enable global interrupts
    sei();
}

// --- SPI Interrupt Service Routine ---
ISR(SPI_STC_vect) {
    char received = SPDR;

    // Respond with 'A' if received command is '1', else 'C'
    if (received == '1') {
        response_char = 'A';
    } else {
        response_char = 'C';
    }

    SPDR = response_char;
}

// --- Main program loop ---
int main(void) {
    init_slave1_spi();

    while (1) {
        // Main loop does nothing; all work handled by ISR
    }
}
