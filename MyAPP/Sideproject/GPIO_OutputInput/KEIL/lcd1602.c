
#include "NuMicro.h"

//--- lcd1602.c
void lcd_print(uint8_t pos, char *p);
void lcd_init(void);
//------------

#define RS  PC14
#define RW  PB14
#define E   PB15


/* =========================================================
 * DWT delay
 * Cortex-M4 cycle counter
 * Does not use SysTick, so it will not conflict with RTX
 * =========================================================
 */

static void delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


static void delay_us(uint32_t us)
{
    uint32_t start;
    uint32_t ticks;

    start = DWT->CYCCNT;

    ticks = (SystemCoreClock / 1000000UL) * us;

    while((uint32_t)(DWT->CYCCNT - start) < ticks)
    {
        __NOP();
    }
}


static void delay_ms(uint32_t ms)
{
    while(ms--)
    {
        delay_us(1000);
    }
}


/* =========================================================
 * LCD low level
 * =========================================================
 */

static void lcd_write_bus(uint8_t data)
{
    PA->DOUT =
        (PA->DOUT & ~0xFFUL) |
        ((uint32_t)data);
}


void lcd_cmd(char c)
{
    RS = 0;
    RW = 0;

    lcd_write_bus((uint8_t)c);

    /*
     * HD44780 Enable pulse
     */
    E = 1;
    delay_us(2);

    E = 0;

    /*
     * A typical instruction requires approximately >= 37 us
     * Use 50 us here for additional timing margin
     */
    if((uint8_t)c == 0x01 || (uint8_t)c == 0x02)
    {
        /*
         * Clear / Home instructions require more time
         */
        delay_ms(2);
    }
    else
    {
        delay_us(50);
    }
}


void lcd_put(char c)
{
    RS = 1;
    RW = 0;

    lcd_write_bus((uint8_t)c);

    E = 1;
    delay_us(2);

    E = 0;

    delay_us(50);
}


/* =========================================================
 * LCD high level
 * =========================================================
 */

void lcd_print(uint8_t pos, char *p)
{
    lcd_cmd(0x80 | pos);

    while(*p)
    {
        lcd_put(*p++);
    }
}


void lcd_init(void)
{
    /* GPIO */
    GPIO_SetMode(PC, BIT14, GPIO_MODE_OUTPUT);

    GPIO_SetMode(
        PB,
        BIT14 | BIT15,
        GPIO_MODE_OUTPUT
    );

    GPIO_SetMode(
        PA,
        BIT0 |
        BIT1 |
        BIT2 |
        BIT3 |
        BIT4 |
        BIT5 |
        BIT6 |
        BIT7,
        GPIO_MODE_OUTPUT
    );

    /*
     * Prevent PA8 ~ PA15 from being affected by PA->DOUT writes
     */
    GPIO_ENABLE_DOUT_MASK(PA, 0xFF00);

    RS = 0;
    RW = 0;
    E  = 0;

    delay_init();

    /*
     * LCD power-on wait
     */
    delay_ms(20);


    lcd_cmd(0x30);
    delay_ms(5);

    lcd_cmd(0x30);
    delay_us(200);

    lcd_cmd(0x30);
    delay_us(200);


    /*
     * 8-bit
     * 2 lines
     * 5x8 font
     */
    lcd_cmd(0x38);


    /*
     * Display ON
     * Cursor ON
     */
    lcd_cmd(0x0E);


    /*
     * Clear display
     */
    lcd_cmd(0x01);


    /*
     * Entry mode
     * cursor auto increment
     */
    lcd_cmd(0x06);


    lcd_print(0x00, "                ");
    lcd_print(0x40, "                ");
}
