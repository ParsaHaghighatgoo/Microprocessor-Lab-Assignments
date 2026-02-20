// --- Include necessary headers ---
#include <avr/io.h>
#include <util/delay.h>
#include <LCD.h>

// --- SPI pin definitions ---
#define PIN_MOSI    PB5
#define PIN_MISO    PB6
#define PIN_SCK     PB7

#define PORT_SS1   PORTB
#define DDR_SS1    DDRB
#define PIN_SS1    PB4

#define PORT_SS2   PORTC
#define DDR_SS2    DDRC
#define PIN_SS2    PC0

// --- SPI control bits ---
#define SPI_ENABLE_BIT   (1 << SPE)
#define SPI_MASTER_BIT   (1 << MSTR)
#define SPI_CLK_DIV16    (1 << SPR0)

// --- Slave selection macros ---
#define ACTIVATE_SLAVE1()  do { PORTB &= ~(1 << PIN_SS1); PORTC |=  (1 << PIN_SS2); } while(0)
#define ACTIVATE_SLAVE2()  do { PORTB |=  (1 << PIN_SS1); PORTC &= ~(1 << PIN_SS2); } while(0)
#define DEACTIVATE_SLAVE1()  (PORTB |= (1 << PIN_SS1))
#define DEACTIVATE_SLAVE2()  (PORTC |= (1 << PIN_SS2))

// --- Placeholder byte for SPI ---
#define SPI_DUMMY_BYTE 0xFF

// --- Function to print string on LCD ---
void lcd_print_str(const char *text) {
    while (*text) {
        LCD_write(*text++);
    }
}

// --- Show slave data on LCD screen ---
void lcd_show_slave_data(char data1, char data2) {
    LCD_cmd(0x01);        // Clear LCD display
    LCD_cmd(0x80);        // Move cursor to line 1
    lcd_print_str("Slave1 Data: ");
    LCD_write(data1);
    
    LCD_cmd(0xC0);        // Move cursor to line 2
    lcd_print_str("Slave2 Data: ");
    LCD_write(data2);
}

// --- Initialize SPI Master hardware ---
void spi_master_setup(void) {
    // Set MOSI, SCK and Slave 1 pins as outputs
    DDR_SS1 |= (1 << PIN_MOSI) | (1 << PIN_SCK) | (1 << PIN_SS1);
    // Set MISO pin as input
    DDR_SS1 &= ~(1 << PIN_MISO);
    // Set Slave 2 pin as output
    DDR_SS2 |= (1 << PIN_SS2);

    // Enable SPI in Master mode with clk/16
    SPCR = SPI_ENABLE_BIT | SPI_MASTER_BIT | SPI_CLK_DIV16;
}

// --- Transmit a byte to selected slave and receive response ---
char spi_master_transfer(char sendByte, uint8_t slaveNum) {
    // Choose slave
    switch (slaveNum) {
        case 1: ACTIVATE_SLAVE1(); break;
        case 2: ACTIVATE_SLAVE2(); break;
        default: return 0; // Invalid slave
    }

    // Start transmission
    SPDR = sendByte;
    while (!(SPSR & (1 << SPIF)));  // Wait until done

    _delay_us(20); // Small delay to stabilize

    // Send dummy to receive response byte
    SPDR = SPI_DUMMY_BYTE;
    while (!(SPSR & (1 << SPIF)));

    char received = SPDR;

    // Deactivate the slave
    if (slaveNum == 1)
        DEACTIVATE_SLAVE1();
    else if (slaveNum == 2)
        DEACTIVATE_SLAVE2();

    return received;
}

int main(void) {
    // Configure LCD control pins
    DDRA = 0xFF;
    DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2);
    init_LCD();

    char slave1_resp = ' ';
    char slave2_resp = ' ';

    // Configure button pins as inputs with pull-ups enabled
    DDRD &= ~((1 << PD3) | (1 << PD4) | (1 << PD5));
    PORTD |= (1 << PD3) | (1 << PD4) | (1 << PD5);

    spi_master_setup();
    lcd_show_slave_data(slave1_resp, slave2_resp);

    while (1) {
        if (!(PIND & (1 << PD3))) {
            slave1_resp = spi_master_transfer('1', 1);
            slave2_resp = spi_master_transfer('1', 2);
            lcd_show_slave_data(slave1_resp, slave2_resp);
            _delay_ms(300);
        }
        else if (!(PIND & (1 << PD4))) {
            slave1_resp = spi_master_transfer('2', 1);
            slave2_resp = spi_master_transfer('2', 2);
            lcd_show_slave_data(slave1_resp, slave2_resp);
            _delay_ms(300);
        }
        else if (!(PIND & (1 << PD5))) {
            slave1_resp = spi_master_transfer('X', 1);
            slave2_resp = spi_master_transfer('X', 2);
            lcd_show_slave_data(slave1_resp, slave2_resp);
            _delay_ms(300);
        }
    }
}
