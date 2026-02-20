#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <stdint.h>
#include <string.h>

// ------------------ LCD on PORTC, control on PORTA ------------------
#define LCD_DATA PORTC
#define ctrl PORTA
#define en PA2
#define rw PA1
#define rs PA0

void LCD_cmd(unsigned char cmd);
void LCD_write(unsigned char data);
void init_LCD(void)
{
    LCD_cmd(0x38); // 8-bit mode
    _delay_ms(1);
    LCD_cmd(0x01); // clear screen
    _delay_ms(1);
    LCD_cmd(0x0E); // cursor on
    _delay_ms(1);
    LCD_cmd(0x80); // cursor to first line
    _delay_ms(1);
}

void LCD_cmd(unsigned char cmd)
{
    LCD_DATA = cmd;
    ctrl = (0<<rs) | (0<<rw) | (1<<en);
    _delay_ms(1);
    ctrl = 0x00;
    _delay_ms(2);
}

void LCD_write(unsigned char data)
{
    LCD_DATA = data;
    ctrl = (1<<rs) | (0<<rw) | (1<<en);
    _delay_ms(1);
    ctrl = (1<<rs) | (0<<rw) | (0<<en);
    _delay_ms(2);
}

// ------------------ LED Control (common-anode bi-color on PB3/PB4) ------------------
static inline void LED_Off(void) {
    DDRB |= (1<<PB3) | (1<<PB4);
    PORTB |= (1<<PB3) | (1<<PB4);
}
static inline void LED_Red(void) {
    DDRB |= (1<<PB3) | (1<<PB4);
    PORTB &= ~(1<<PB3);
    PORTB |=  (1<<PB4);
}
static inline void LED_Green(void) {
    DDRB |= (1<<PB3) | (1<<PB4);
    PORTB &= ~(1<<PB4);
    PORTB |=  (1<<PB3);
}

// --- LED pulse helper (non-blocking, color-aware) ---
typedef enum { LED_NONE, LED_GREEN_C, LED_RED_C } led_color_t;

volatile uint32_t ms_ticks = 0;
static volatile uint32_t led_off_at_ms = 0;
static volatile led_color_t led_active = LED_NONE;

static inline void LED_Pulse(led_color_t color, uint16_t duration_ms) {
    switch (color) {
        case LED_RED_C:   LED_Red();   break;
        case LED_GREEN_C: LED_Green(); break;
        default:          LED_Off();   break;
    }
    led_active = color;
    led_off_at_ms = ms_ticks + duration_ms;
}
static inline void LED_Service(void) {
    if (led_off_at_ms && (int32_t)(ms_ticks - led_off_at_ms) >= 0) {
        LED_Off();
        led_off_at_ms = 0;
        led_active = LED_NONE;
    }
}

// ------------------ USART ------------------
static unsigned long g_current_baud = 9600UL;

void USART_Init(unsigned long baud)
{
    g_current_baud = baud;

    // Auto U2X for higher rates at 8 MHz (improves accuracy for >=57.6k)
    if (baud >= 57600UL) {
        UCSRA |= (1<<U2X);
        uint16_t ubrr = (uint16_t)(F_CPU/8/baud - 1);
        UBRRH = (uint8_t)(ubrr>>8);
        UBRRL = (uint8_t)ubrr;
    } else {
        UCSRA &= ~(1<<U2X);
        uint16_t ubrr = (uint16_t)(F_CPU/16/baud - 1);
        UBRRH = (uint8_t)(ubrr>>8);
        UBRRL = (uint8_t)ubrr;
    }

    UCSRB = (1<<RXEN) | (1<<TXEN) | (1<<RXCIE);         // enable RX interrupt for wake
    UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);       // 8N1
}

static inline void USART_Transmit(uint8_t data)
{
    while (!(UCSRA & (1<<UDRE)));
    UDR = data;
}

// NOTE: We’ll do non-blocking RX with status, not this blocking getter.

// ------------------ ADC ------------------
void ADC_Init(void)
{
    ADMUX  = (1<<REFS0); // AVcc ref
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1); // prescaler 64
}

uint16_t ADC_Read(uint8_t ch)
{
    ch &= 0x07;
    ADMUX = (ADMUX & 0xF8) | ch; // select channel
    ADCSRA |= (1<<ADSC);         // start
    while (ADCSRA & (1<<ADSC));  // wait
    return ADC;                  // read 10-bit
}

// ------------------ Codec + Framing ------------------
#define SYNC_CODE   0xAA
#define CODEC_KEY   0x5A
#define MAX_PAYLOAD 32  // payload bytes (not counting MSG_ID)

static inline uint8_t xenc(uint8_t b) { return b ^ CODEC_KEY; }
static inline uint8_t xdec(uint8_t b) { return b ^ CODEC_KEY; }

// Dallas/Maxim CRC8 (poly 0x31, reflected 0x8C), init 0x00
static uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

// ------------------ Timer0: 10 ms tick for Channel Manager ------------------
ISR(TIMER0_COMP_vect) { ms_ticks += 10; }

static void Timer0_10ms_Init(void) {
    // CTC, OCR0 for ~10ms @ 8MHz with /1024: 8e6/1024 ≈ 7812.5 Hz → 78 counts ≈ 9.98ms
    TCCR0 = (1<<WGM01) | (1<<CS02) | (1<<CS00); // CTC, presc 1024
    OCR0  = 78;
    TIMSK |= (1<<OCIE0); // enable compare match
}

// ------------------ Channels (timer-managed selection with hysteresis) ------------------
static const unsigned long BAUD_TABLE[8] = {
    2400UL, 4800UL, 9600UL, 14400UL, 19200UL, 38400UL, 57600UL, 115200UL
};

// Channel Manager state
static uint8_t  ch_current = 0xFF;   // active bin
static uint8_t  ch_wanted  = 0xFF;   // candidate bin
static uint32_t ch_since_ms = 0;     // when candidate started
static const uint16_t HYST = 16;     // ADC hysteresis around boundaries

// Average 4 samples of ADC3
static uint16_t adc3_avg4(void) {
    uint32_t acc = 0;
    for (uint8_t i=0;i<4;i++) acc += ADC_Read(3);
    return (uint16_t)(acc >> 2);
}

// Map averaged ADC to bin with hysteresis around boundaries
static uint8_t map_adc_to_bin_hyst(uint16_t adc, uint8_t cur_bin) {
    // boundaries for 8 bins (0..1023). Each bin = 128 wide: 0,128,256,...,1024
    uint16_t bnd[9]; for (uint8_t i=0;i<9;i++) bnd[i] = (uint16_t)i * 128;

    if (cur_bin > 7) {
        // no current → plain quantize
        uint8_t q = (uint8_t)(adc >> 7);
        return (q > 7) ? 7 : q;
    }

    // Hysteresis: don’t leave current bin until passing boundary +/- HYST
    uint16_t low  = bnd[cur_bin];
    uint16_t high = bnd[cur_bin+1];

    // stay if within [low+HYST, high−HYST]
    if (adc >= (low + HYST) && adc <= (high - HYST)) return cur_bin;

    // otherwise allow transition to neighbor bin if crossed past the margin
    if (adc < (low + HYST) && cur_bin > 0)        return (uint8_t)(cur_bin - 1);
    if (adc > (high - HYST) && cur_bin < 7)       return (uint8_t)(cur_bin + 1);

    return cur_bin; // default stay
}

// Re-init UART to a specific bin (internal helper)
static void uart_switch_to_bin(uint8_t bin) {
    if (bin > 7) bin = 7;
    uint8_t s = SREG; cli();

    // Disable during change
    UCSRB = 0;
    _delay_ms(2);

    // Flush any leftover bytes from previous baud
    while (UCSRA & (1<<RXC)) { volatile uint8_t d = UDR; (void)d; }
    UCSRA |= (1<<TXC); // clear TXC by writing 1

    // Program UBRR/U2X
    unsigned long baud = BAUD_TABLE[bin];
    if (baud >= 57600UL) {
        UCSRA |= (1<<U2X);
        uint16_t ubrr = (uint16_t)(F_CPU/8/baud - 1);
        UBRRH = (uint8_t)(ubrr>>8);
        UBRRL = (uint8_t)ubrr;
    } else {
        UCSRA &= ~(1<<U2X);
        uint16_t ubrr = (uint16_t)(F_CPU/16/baud - 1);
        UBRRH = (uint8_t)(ubrr>>8);
        UBRRL = (uint8_t)ubrr;
    }
    UCSRB = (1<<RXEN) | (1<<TXEN) | (1<<RXCIE);
    UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);

    SREG = s;
    _delay_ms(2);
    g_current_baud = baud;
}

// Poll the channel manager ~every 10 ms; commit after 60 ms stable OR big jump >=2 bins
static void channel_manager_poll(void) {
    static uint32_t last_poll = 0;
    uint32_t now = ms_ticks;
    if ((now - last_poll) < 10) return;  // run ~every 10ms
    last_poll = now;

    uint16_t a = adc3_avg4();
    uint8_t next = map_adc_to_bin_hyst(a, (ch_current==0xFF)? (uint8_t)(a>>7) : ch_current);
    if (next > 7) next = 7;

    if (ch_wanted != next) {
        ch_wanted = next;
        ch_since_ms = now;
    }

    uint8_t diff = (ch_current==0xFF) ? 0 : (uint8_t)((ch_wanted>ch_current)?(ch_wanted-ch_current):(ch_current-ch_wanted));
    if (ch_current == 0xFF || diff >= 2 || (now - ch_since_ms) >= 60) {
        if (ch_current != ch_wanted) {
            uart_switch_to_bin(ch_wanted);
            ch_current = ch_wanted;
        }
    }
}

// Helper to display the current channel index on LCD (optional)
static void lcd_show_channel(uint8_t mode_is_tx) {
    LCD_cmd(0x01);
    if (mode_is_tx) { LCD_write('T'); LCD_write('x'); }
    else            { LCD_write('R'); LCD_write('x'); }
    LCD_write(' ');
    LCD_write('#'); LCD_write('0' + (ch_current>7?7:ch_current));
}

// ------------------ Message Catalog ------------------
typedef struct {
    uint8_t id;
    const char *text;
} msg_entry_t;

enum {
    MSG_H = 'H',  // Help
    MSG_O = 'O',  // Over
    MSG_R = 'R',  // Roger That
    MSG_S = 'S',  // Stand By
    // Extra examples
    MSG_A = 'A',  // Affirmative
    MSG_N = 'N',  // Negative
    MSG_L = 'L',  // Location?
    MSG_M = 'M',  // Meet
    MSG_C = 'C',  // Cancel
};

static const msg_entry_t MSG_TABLE[] = {
    { MSG_H, "Help" },
    { MSG_O, "Over" },
    { MSG_R, "Roger That" },
    { MSG_S, "Stand By" },
    { MSG_A, "Affirmative" },
    { MSG_N, "Negative" },
    { MSG_L, "Location?" },
    { MSG_M, "Meet" },
    { MSG_C, "Cancel" },
};

static const char* msg_lookup(uint8_t id) {
    for (uint8_t i = 0; i < (sizeof(MSG_TABLE)/sizeof(MSG_TABLE[0])); i++) {
        if (MSG_TABLE[i].id == id) return MSG_TABLE[i].text;
    }
    return NULL;
}

// ------------------ Sleep ------------------
static inline void go_to_sleep(void)
{
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
}

// ------------------ Framed Send & Receive ------------------
// Frame on wire: SYNC | ENC(LEN) | ENC(MSG_ID) | ENC(PAYLOAD[LEN-1]) | ENC(CRC)
// CRC over plaintext bytes: [LEN][MSG_ID][PAYLOAD...]

static void framed_send(uint8_t msg_id, const uint8_t *payload, uint8_t plen);

void sendMessage(char code)
{
    framed_send((uint8_t)code, NULL, 0);
}

static void framed_send(uint8_t msg_id, const uint8_t *payload, uint8_t plen)
{
    if (plen > MAX_PAYLOAD) plen = MAX_PAYLOAD;
    uint8_t len = (uint8_t)(1 + plen); // include MSG_ID

    // Build plaintext buffer for CRC (LEN + content)
    uint8_t buf[1 + 1 + MAX_PAYLOAD]; // LEN + [MSG_ID + PAYLOAD]
    buf[0] = len;
    buf[1] = msg_id;
    for (uint8_t i = 0; i < plen; i++) buf[2 + i] = payload[i];

    uint8_t crc = crc8(buf, (uint8_t)(1 + len));

    _delay_ms(3);                       // settle after potential rate changes
    USART_Transmit(SYNC_CODE);          // preamble SYNC

    // XOR-encoded bytes
    USART_Transmit(xenc(buf[0]));                    // LEN
    USART_Transmit(xenc(buf[1]));                    // MSG_ID
    for (uint8_t i = 0; i < plen; i++)               // PAYLOAD
        USART_Transmit(xenc(buf[2 + i]));
    USART_Transmit(xenc(crc));                       // CRC
}

// -------- NEW: Non-blocking RX parser with error/timeout -> RED --------
typedef enum {
    RX_WAIT_SYNC = 0,
    RX_GOT_LEN,
    RX_GOT_MSG,
    RX_GET_PAYLOAD,
    RX_GET_CRC
} rx_state_t;

static volatile rx_state_t rx_state = RX_WAIT_SYNC;
static uint8_t rx_len = 0;
static uint8_t rx_msg_id = 0;
static uint8_t rx_plen = 0;
static uint8_t rx_payload[MAX_PAYLOAD];
static uint8_t rx_pi = 0;
static uint32_t rx_last_byte_ms = 0;

#define RX_TIMEOUT_MS 50

// Pull one byte if available; return 1 if a byte was read (and stored to *b), else 0.
// Also returns UART status flags via *status (FE/DOR/PE bits preserved).
static inline uint8_t usart_read_byte_nb(uint8_t *b, uint8_t *status) {
    if (UCSRA & (1<<RXC)) {
        uint8_t s = UCSRA;   // read status first
        uint8_t d = UDR;     // then the data
        if (status) *status = s;
        if (b) *b = d;
        return 1;
    }
    return 0;
}

static void rx_parser_service(void)
{
    // Timeout mid-frame -> RED + reset
    if (rx_state != RX_WAIT_SYNC) {
        if ((ms_ticks - rx_last_byte_ms) > RX_TIMEOUT_MS) {
            LED_Pulse(LED_RED_C, 40);
            rx_state = RX_WAIT_SYNC;
        }
    }

    uint8_t s, d;
    while (usart_read_byte_nb(&d, &s)) {
        rx_last_byte_ms = ms_ticks;

        // Any UART error -> RED and reset
        if (s & ((1<<FE)|(1<<DOR)|(1<<PE))) {
            LED_Pulse(LED_RED_C, 40);
            rx_state = RX_WAIT_SYNC;
            continue;
        }

        switch (rx_state) {
        case RX_WAIT_SYNC:
            if (d == SYNC_CODE) {
                rx_state = RX_GOT_LEN;
            } else {
                // Noise while waiting for SYNC -> show RED quickly
                LED_Pulse(LED_RED_C, 20);
            }
            break;

        case RX_GOT_LEN: {
            rx_len = xdec(d);
            if (rx_len == 0 || rx_len > (1 + MAX_PAYLOAD)) {
                LED_Pulse(LED_RED_C, 40);
                rx_state = RX_WAIT_SYNC;
            } else {
                rx_plen = (uint8_t)(rx_len - 1);
                rx_state = RX_GOT_MSG;
            }
        } break;

        case RX_GOT_MSG:
            rx_msg_id = xdec(d);
            rx_pi = 0;
            if (rx_plen == 0) {
                rx_state = RX_GET_CRC;
            } else {
                rx_state = RX_GET_PAYLOAD;
            }
            break;

        case RX_GET_PAYLOAD:
            rx_payload[rx_pi++] = xdec(d);
            if (rx_pi >= rx_plen) {
                rx_state = RX_GET_CRC;
            }
            break;

        case RX_GET_CRC: {
            uint8_t rx_crc = xdec(d);
            uint8_t checkbuf[1 + 1 + MAX_PAYLOAD];
            checkbuf[0] = rx_len;
            checkbuf[1] = rx_msg_id;
            for (uint8_t i=0;i<rx_plen;i++) checkbuf[2+i] = rx_payload[i];
            if (crc8(checkbuf, (uint8_t)(1 + rx_len)) != rx_crc) {
                LED_Pulse(LED_RED_C, 40);
            } else {
                // OK -> show message
                LED_Pulse(LED_GREEN_C, 60);
                LCD_cmd(0x01);
                const char *msg = msg_lookup(rx_msg_id);
                if (msg) {
                    for (uint8_t i = 0; msg[i]; i++) LCD_write(msg[i]);
                } else {
                    const char hexmap[] = "0123456789ABCDEF";
                    LCD_write('I'); LCD_write('D'); LCD_write(' ');
                    LCD_write('0'); LCD_write('x');
                    LCD_write(hexmap[(rx_msg_id >> 4) & 0xF]);
                    LCD_write(hexmap[rx_msg_id & 0xF]);
                }
                if (rx_plen > 0) {
                    LCD_write(' ');
                    uint8_t to_show = (rx_plen > 14) ? 14 : rx_plen;
                    for (uint8_t i=0;i<to_show;i++) LCD_write((char)rx_payload[i]);
                }
            }
            rx_state = RX_WAIT_SYNC;
        } break;
        } // switch
    } // while byte available
}

// ------------------ Interrupts ------------------
ISR(INT0_vect) { /* wakes from sleep */ }
ISR(INT1_vect) { /* wakes from sleep */ }
ISR(INT2_vect) { /* wakes from sleep */ }
ISR(USART_RXC_vect) { /* wakes from sleep */ }

// ------------------ Timer1: ~1 Hz for standby flag ------------------
volatile uint8_t standby_counter = 0;
volatile uint8_t send_standby_flag = 0;

ISR(TIMER1_COMPA_vect)
{
    standby_counter++;
    if (standby_counter >= 3) {  // ~1 min (with 1 Hz tick)
        send_standby_flag = 1;
    }
}

void Timer1_Init(void)
{
    TCCR1B |= (1 << WGM12);               // CTC mode
    OCR1A = 7812;                         // ~1 Hz with 8 MHz / 1024
    TIMSK |= (1 << OCIE1A);               // Enable compare A interrupt
    TCCR1B |= (1 << CS12) | (1 << CS10);  // Prescaler 1024
}

// ------------------ Button handling (multi-function) ------------------
#define BTN1_MASK 0x01   // PD2
#define BTN2_MASK 0x02   // PD3
#define BTN3_MASK 0x04   // PB2
#define DEBOUNCE_MS 20
#define LONG_MS     50  // long-press threshold

static inline uint8_t raw_buttons_mask(void) {
    uint8_t m = 0;
    if (!(PIND & (1<<PD2))) m |= BTN1_MASK; // PD2 pressed?
    if (!(PIND & (1<<PD3))) m |= BTN2_MASK; // PD3 pressed?
    if (!(PINB & (1<<PB2))) m |= BTN3_MASK; // PB2 pressed?
    return m;
}

// Debounced reading using ms_ticks
static uint8_t buttons_debounced(void) {
    static uint8_t last_raw = 0, stable = 0;
    static uint32_t last_change = 0;

    uint8_t r = raw_buttons_mask();
    if (r != last_raw) {
        last_raw = r;
        last_change = ms_ticks;
    }
    if ((ms_ticks - last_change) >= DEBOUNCE_MS) {
        stable = r;
    }
    return stable;
}

// Map button mask + press duration -> message ID
static uint8_t map_press_to_msg(uint8_t mask, uint16_t duration_ms) {
    // Chords (two or three buttons)
    if (mask == (BTN1_MASK | BTN2_MASK)) return MSG_S; // PD2+PD3
    if (mask == (BTN1_MASK | BTN3_MASK)) return MSG_L; // PD2+PB2
    if (mask == (BTN2_MASK | BTN3_MASK)) return MSG_M; // PD3+PB2
    if (mask == (BTN1_MASK | BTN2_MASK | BTN3_MASK)) return MSG_S; // all 3 -> Stand By (example)

    // Singles with short/long
    if (mask == BTN1_MASK) return (duration_ms >= LONG_MS) ? MSG_A : MSG_H; // PD2
    if (mask == BTN2_MASK) return (duration_ms >= LONG_MS) ? MSG_N : MSG_O; // PD3
    if (mask == BTN3_MASK) return (duration_ms >= LONG_MS) ? MSG_C : MSG_R; // PB2

    return 0; // no-op
}

static void send_and_show_tx(uint8_t msg_id) {
    sendMessage((char)msg_id);   // sends only the ID byte
    LCD_cmd(0x01);
    LCD_write((char)msg_id);     // show 'H', 'O', 'R', ...
    standby_counter = 0;
    LED_Pulse(LED_GREEN_C, 60);
}

// Tracks a full gesture (single or chord) until *all* buttons released
static void buttons_service_sender(void) {
    static uint8_t down_mask = 0;
    static uint32_t down_since = 0;

    uint8_t st = buttons_debounced();

    if (down_mask == 0) {
        // idle → new press
        if (st) {
            down_mask = st;             // remember which buttons started it
            down_since = ms_ticks;
        }
    } else {
        // pressed: allow “chording” (add any new presses to the original set)
        if (st) {
            down_mask |= st;
        } else {
            // all released → resolve gesture
            uint16_t dur = (uint16_t)(ms_ticks - down_since);
            uint8_t msg = map_press_to_msg(down_mask, dur);
            if (msg) send_and_show_tx(msg);
            down_mask = 0;
        }
    }
}

// ------------------ Main ------------------
int main(void)
{
    // LCD
    DDRA |= (1<<PA0)|(1<<PA1)|(1<<PA2);
    DDRC  = 0xFF;

    // USART pins
    DDRD &= ~(1<<PD0); // RX input
    DDRD |=  (1<<PD1); // TX output

    // Buttons as inputs (PD2, PD3, PD5; PB2). Enable pull-ups.
    DDRD &= ~((1<<PD2)|(1<<PD3)|(1<<PD5));
    DDRB &= ~(1<<PB2);
    PORTD |= (1<<PD5)|(1<<PD2)|(1<<PD3);
    PORTB |= (1<<PB2);

    // External interrupts config (falling edges for INT0/1)
    MCUCR |= (1<<ISC01)|(1<<ISC11);
    GICR  |= (1<<INT0)|(1<<INT1)|(1<<INT2);

    sei();

    init_LCD();
    ADC_Init();
    LED_Off();

    // Initialize channel once based on ADC, then init UART
    uint16_t a0 = adc3_avg4();
    ch_current = (uint8_t)((a0>>7) > 7 ? 7 : (a0>>7));
    uart_switch_to_bin(ch_current);   // sets UART + U2X as needed

    Timer1_Init();
    Timer0_10ms_Init();               // 10 ms tick for channel manager

    // UI init
    LCD_cmd(0x01);
    LCD_write('R'); LCD_write('x'); LCD_write(' ');
    LCD_write('#'); LCD_write('0' + ch_current);

    uint8_t mode = 0; // 0 = Receiver (default), 1 = Sender
    uint8_t lastButtonState = (PIND & (1<<PD5));
    static uint8_t last_idx_shown = 0xFF;

    while (1)
    {
        // Track and apply channel changes safely
        channel_manager_poll();

        // Non-blocking LED service
        LED_Service();

        // --- RX parser service (drains UART & handles errors/timeout) ---
        if (mode == 0) {
            rx_parser_service();
        }

        // Show channel index (briefly) after any change
        if (last_idx_shown != ch_current) {
            last_idx_shown = ch_current;
            lcd_show_channel(/*mode_is_tx*/ (mode==1));
            _delay_ms(60);
        }

        // --- Mode toggle button (PD5) ---
        uint8_t currentButtonState = (PIND & (1<<PD5));
        if (lastButtonState != currentButtonState) {
            _delay_ms(50); // debounce
            if (!(PIND & (1<<PD5))) { // pressed => toggle
                mode ^= 1;
                lcd_show_channel(/*mode_is_tx*/ (mode==1));
                // Reset RX state when leaving Rx/entering Rx
                rx_state = RX_WAIT_SYNC;
            }
        }
        lastButtonState = currentButtonState;

        // --- Standby auto-send (if in Tx mode) ---
        if (send_standby_flag && mode) {
            send_standby_flag = 0;
            LCD_cmd(0x01);
            LCD_write('S');
            sendMessage(MSG_S);       // encoded + framed
            LED_Pulse(LED_GREEN_C, 90);
            standby_counter = 0;
        }

        if (mode == 1) { // Sender
            buttons_service_sender();
        } else {
            // Light sleep to keep timers/USART running
            go_to_sleep();
        }
    }
}
