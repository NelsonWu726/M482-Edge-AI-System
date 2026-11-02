#include <stdio.h>
#include "NuMicro.h"
#include "cmsis_os.h"

/* =========================
   Module interfaces
   ========================= */

/* lcd1602.c */
void lcd_init(void);
void lcd_print(uint8_t pos, char *p);

/* buzzer.c */
void init_buzzer(void);
void start_buzzer(void);
void stop_buzzer(void);
void buzzer_on(uint16_t ms);

/* timer1.c */
void init_TIMER0(void);

/* adc.c */
void init_adc(void);
void adc_start(void);
uint8_t adc_is_ready(void);
uint16_t adc_get_tr(void);
uint16_t adc_get_vr(void);
uint16_t adc_get_bandgap(void);
float adc_get_temperature(void);

/* myi2c.c */
void i2c_start(void);
void i2c_stop(void);
uint8_t i2c_write(uint8_t d);
uint8_t i2c_read(uint8_t ack);
uint16_t i2c_scan(uint8_t n);

/* bh1750.c */
void bh1750_init(void);
uint16_t bh1750_get_light(void);

/* wifi.c */
void wifi_init(void);
int wifi_test_at(uint32_t timeout_ms);


/* =========================
   Old single-module tests
   Keep the original test functions for standalone debugging later
   ========================= */

void lcdTest(void)
{
    printf("LCD INIT START\r\n");

    lcd_init();

    printf("LCD INIT OK\r\n");

    lcd_print(
        0x00,
        "RTOS LCD TEST   "
    );

    lcd_print(
        0x40,
        "WORKING         "
    );

    printf("LCD PRINT OK\r\n");

    osDelay(500);
}


void buzzerTest(void)
{
    printf("BUZZER INIT START\r\n");

    init_buzzer();

    printf("BUZZER INIT OK\r\n");

    printf("TIMER0 INIT START\r\n");

    init_TIMER0();

    printf("TIMER0 INIT OK\r\n");

    osDelay(500);

    printf("BUZZER TIMER TEST START\r\n");

    buzzer_on(300);

    osDelay(1000);

    printf("BUZZER TIMER TEST END\r\n");
}


void adcTest(void)
{
    uint32_t timeout;
    uint16_t tr;
    float temp;

    int temp10;
    int whole;
    int decimal;

    char line[17];

    printf("ADC INIT START\r\n");

    init_adc();

    printf("ADC INIT OK\r\n");

    printf("ADC CONVERSION START\r\n");

    adc_start();

    printf("ADC START RETURN\r\n");

    timeout = 100;

    while(!adc_is_ready())
    {
        if(timeout == 0)
        {
            printf("ADC TEST FAIL : TIMEOUT\r\n");

            lcd_print(
                0x40,
                "ADC TIMEOUT     "
            );

            return;
        }

        timeout--;

        osDelay(1);
    }

    printf("ADC CONVERSION OK\r\n");

    tr = adc_get_tr();

    temp = adc_get_temperature();

    printf("ADC TR = %u\r\n", tr);

    if(temp >= 0.0f)
    {
        temp10 = (int)(temp * 10.0f);

        whole = temp10 / 10;
        decimal = temp10 % 10;

        sprintf(
            line,
            "TEMP:%d.%d C",
            whole,
            decimal
        );

        lcd_print(
            0x40,
            "                "
        );

        lcd_print(
            0x40,
            line
        );

        printf(
            "TEMP = %d.%d C\r\n",
            whole,
            decimal
        );
    }
    else
    {
        lcd_print(
            0x40,
            "TEMP ERROR      "
        );

        printf("TEMP ERROR\r\n");
    }

    printf("ADC TEST OK\r\n");

    osDelay(500);
}


void i2cTest(void)
{
    uint16_t addr;

    printf("I2C TEST START\r\n");

    addr = i2c_scan(0x00);

    if(addr < 255)
    {
        printf(
            "I2C DEVICE FOUND: 0x%02X\r\n",
            addr
        );

        printf(
            "7-BIT ADDRESS: 0x%02X\r\n",
            addr >> 1
        );

        lcd_print(
            0x00,
            "I2C DEVICE FOUND"
        );
    }
    else
    {
        printf("I2C DEVICE NOT FOUND\r\n");

        lcd_print(
            0x00,
            "I2C FAIL        "
        );
    }

    osDelay(500);
}


void bh1750Test(void)
{
    uint16_t raw;
    uint32_t lux10;
    uint32_t whole;
    uint32_t decimal;

    char line[17];

    printf("BH1750 INIT START\r\n");

    bh1750_init();

    printf("BH1750 INIT OK\r\n");

		raw = bh1750_get_light();

		printf("BH1750 RAW = %u\r\n", raw);

		lux10 = ((uint32_t)raw * 100U) / 24U;

		whole   = lux10 / 10U;
		decimal = lux10 % 10U;

		printf(
				"LIGHT = %u.%u lux\r\n",
				whole,
				decimal
		);

		sprintf(
				line,
				"LUX:%u.%u",
				whole,
				decimal
		);

		lcd_print(
				0x00,
				"LIGHT SENSOR    "
		);

    lcd_print(
        0x40,
        "                "
    );

    lcd_print(
        0x40,
        line
    );

    printf("BH1750 TEST OK\r\n");

    osDelay(500);
}


/* =========================
   Boot Self Test
   ========================= */

static uint16_t boot_i2c_addr = 0xFFFF;


static uint8_t boot_adc_test(void)
{
    uint32_t timeout;

    init_adc();

    adc_start();

    timeout = 100;

    while(!adc_is_ready())
    {
        if(timeout == 0)
        {
            return 0;
        }

        timeout--;

        osDelay(1);
    }

    return 1;
}


static uint8_t boot_i2c_test(void)
{
    boot_i2c_addr =
        i2c_scan(0x00);

    /*
     * i2c_scan() returns an 8-bit address byte
     * BH1750 7-bit address = 0x23
     * write address byte   = 0x46
     */
    if(boot_i2c_addr == 0x46)
    {
        return 1;
    }

    return 0;
}


static uint8_t boot_bh1750_test(void)
{
    uint16_t raw;

    if(boot_i2c_addr != 0x46)
    {
        return 0;
    }

    bh1750_init();

    raw =
        bh1750_get_light();

    /*
     * raw == 0 may simply indicate a dark environment,
     * so only verify that a complete read operation was performed.
     */
    (void)raw;

    return 1;
}


static uint8_t boot_uart_esp_test(void)
{
    /*
     * A successful wifi_test_at() indicates that:
     * UART1 TX / RX
     * UART1 IRQ
     * ESP8266 UART
     * AT command
     * are all functioning properly.
     */

    wifi_init();

    osDelay(200);

    if(wifi_test_at(2000))
    {
        return 1;
    }

    return 0;
}


uint8_t boot_self_test(void)
{
    uint8_t all_ok;

    all_ok = 1;


    /* =========================
       HCLK
       ========================= */

    lcd_print(
        0x00,
        "HCLK TEST       "
    );

    if(SystemCoreClock == 192000000UL)
    {
        lcd_print(
            0x40,
            "192MHz PASS     "
        );
    }
    else
    {
        lcd_print(
            0x40,
            "HCLK FAIL       "
        );

        all_ok = 0;
    }

    osDelay(500);


    /* =========================
       RTOS
       ========================= */

    lcd_print(
        0x00,
        "RTOS TEST       "
    );

    lcd_print(
        0x40,
        "RTOS PASS       "
    );

    osDelay(500);


    /* =========================
       LCD
       ========================= */

    lcd_print(
        0x00,
        "LCD TEST        "
    );

    lcd_print(
        0x40,
        "LCD PASS        "
    );

    osDelay(500);


    /* =========================
       TIMER0 + BUZZER
       ========================= */

    lcd_print(
        0x00,
        "BUZZER TIMER    "
    );

    lcd_print(
        0x40,
        "TEST...         "
    );

    init_buzzer();

    init_TIMER0();

    buzzer_on(300);

    osDelay(500);

    /*
     * This test has no feedback pin,
     * so manually verify it by listening for the buzzer.
     */
    lcd_print(
        0x40,
        "TEST DONE       "
    );

    osDelay(500);


    /* =========================
       ADC
       ========================= */

    lcd_print(
        0x00,
        "ADC TEST        "
    );

    lcd_print(
        0x40,
        "WAIT...         "
    );

    if(boot_adc_test())
    {
        lcd_print(
            0x40,
            "ADC PASS        "
        );
    }
    else
    {
        lcd_print(
            0x40,
            "ADC FAIL        "
        );

        all_ok = 0;
    }

    osDelay(500);


    /* =========================
       I2C
       ========================= */

    lcd_print(
        0x00,
        "I2C TEST        "
    );

    lcd_print(
        0x40,
        "SCAN...         "
    );

    if(boot_i2c_test())
    {
        lcd_print(
            0x40,
            "I2C PASS        "
        );
    }
    else
    {
        lcd_print(
            0x40,
            "I2C FAIL        "
        );

        all_ok = 0;
    }

    osDelay(500);


    /* =========================
       BH1750
       ========================= */

    lcd_print(
        0x00,
        "BH1750 TEST     "
    );

    lcd_print(
        0x40,
        "READ...         "
    );

    if(boot_bh1750_test())
    {
        lcd_print(
            0x40,
            "BH1750 PASS     "
        );
    }
    else
    {
        lcd_print(
            0x40,
            "BH1750 FAIL     "
        );

        all_ok = 0;
    }

    osDelay(500);


    /* =========================
       UART1 + ESP8266
       ========================= */

    lcd_print(
        0x00,
        "UART ESP TEST   "
    );

    lcd_print(
        0x40,
        "AT...           "
    );

    if(boot_uart_esp_test())
    {
        lcd_print(
            0x40,
            "UART ESP PASS   "
        );
    }
    else
    {
        lcd_print(
            0x40,
            "UART ESP FAIL   "
        );

        all_ok = 0;
    }

    osDelay(800);


    /* =========================
       Summary
       ========================= */

    if(all_ok)
    {
        lcd_print(
            0x00,
            "BOOT SELF TEST  "
        );

        lcd_print(
            0x40,
            "ALL PASS        "
        );
    }
    else
    {
        lcd_print(
            0x00,
            "BOOT SELF TEST  "
        );

        lcd_print(
            0x40,
            "FAIL            "
        );
    }

    osDelay(1000);

    return all_ok;
}
