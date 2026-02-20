#define F_CPU 1000000UL
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

uint8_t EEMEM reset_counter_eeprom;  // متغیر ذخیره شده در EEPROM

int main(void)
{
    uint8_t reset_counter = eeprom_read_byte(&reset_counter_eeprom);

    // بررسی اینکه آیا ریست ناشی از WDT بوده یا نه
    if (MCUCSR & (1 << WDRF))
    {
        // افزایش شمارنده
        reset_counter++;
    }
    else
    {
        // اگر ریست ناشی از WDT نبوده، شمارنده را صفر کن
        reset_counter = 0;
    }

    // پاک کردن فلگ WDRF
    MCUCSR = 0x00;

    // ذخیره‌سازی شمارنده در EEPROM
    eeprom_write_byte(&reset_counter_eeprom, reset_counter);

    // تنظیم پایه‌ی PB0 به عنوان خروجی
    DDRB |= (1 << PB0);

    // اگر ۳ بار ریست شده، LED را برای همیشه روشن کن
    if (reset_counter >= 3)
    {
        PORTB |= (1 << PB0);  // روشن کردن LED
        while (1);            // حلقه بی‌نهایت
    }

    // راه‌اندازی WDT برای ۲ ثانیه (بسته به تنظیمات تراشه)
    wdt_enable(WDTO_2S);

    while (1)
    {
        PORTB ^= (1 << PB0);   // چشمک زدن LED
        _delay_ms(500);
        // wdt_reset();           // ریست کردن تایمر واچ‌داگ تا ریست نشه
    }
}
