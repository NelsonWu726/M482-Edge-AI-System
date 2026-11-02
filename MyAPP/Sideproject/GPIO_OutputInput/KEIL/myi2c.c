#include "NuMicro.h"
#include "cmsis_os.h"


/* =========================================================
   myi2c.c

   Software I2C bus:
       SCL = PB5
       SDA = PB4

   BH1750 and MAX30102 share this same bus, so one RTOS
   mutex protects each complete I2C transaction.
   ========================================================= */


/* =========================================================
   Function Prototypes
   ========================================================= */

int i2c_mutex_init(void);

uint8_t i2c_lock(void);
void i2c_unlock(void);

void i2c_start(void);
void i2c_stop(void);

uint8_t i2c_write(uint8_t d);
uint8_t i2c_read(uint8_t ack);

uint16_t i2c_scan(uint8_t n);


/* =========================================================
   Timing
   ========================================================= */

#define T0    35
#define T01   10
#define T1    50
#define T2    100


/* =========================================================
   GPIO
   ========================================================= */

#define SCL     PB5
#define SDA     PB4


/* =========================================================
   I2C Bus Mutex
   ========================================================= */

osMutexDef(i2c_bus_mutex);

static osMutexId g_i2c_mutex = NULL;


/*
 * Create the mutex once after the RTOS kernel has started,
 * before sensor_task / max30102_task are created.
 *
 * Return:
 *     1 = success
 *     0 = failed
 */
int i2c_mutex_init(void)
{
    if(g_i2c_mutex != NULL)
    {
        return 1;
    }


    g_i2c_mutex =
        osMutexCreate(
            osMutex(i2c_bus_mutex)
        );


    if(g_i2c_mutex == NULL)
    {
        return 0;
    }


    return 1;
}


/*
 * Lock the complete shared I2C bus transaction.
 *
 * Return:
 *     1 = mutex obtained
 *     0 = mutex unavailable / not initialized
 */
uint8_t i2c_lock(void)
{
    if(g_i2c_mutex == NULL)
    {
        return 0;
    }


    if(
        osMutexWait(
            g_i2c_mutex,
            osWaitForever
        ) != osOK
    )
    {
        return 0;
    }


    return 1;
}


/*
 * Release the shared I2C bus.
 */
void i2c_unlock(void)
{
    if(g_i2c_mutex != NULL)
    {
        osMutexRelease(
            g_i2c_mutex
        );
    }
}


/* =========================================================
   Byte Helper
   ========================================================= */

typedef union
{
    unsigned char v;

    struct
    {
        unsigned b0:1;
        unsigned b1:1;
        unsigned b2:1;
        unsigned b3:1;
        unsigned b4:1;
        unsigned b5:1;
        unsigned b6:1;
        unsigned b7:1;
    };

} TPU8;


/* =========================================================
   Short Software Delay
   ========================================================= */

static void delay(int n)
{
    volatile int i;

    for(i = 0; i < n; i++)
    {
    }
}


/* =========================================================
   I2C START

   IMPORTANT:
   i2c_start() itself does NOT lock the mutex.
   The driver must call i2c_lock() before the full
   START -> data -> STOP transaction.
   ========================================================= */

void i2c_start(void)
{
    /*
     * SDA / SCL are PB4 / PB5.
     *
     * Use PB here, not PF.
     */
    GPIO_SetMode(
        PB,
        BIT4 | BIT5,
        GPIO_MODE_OPEN_DRAIN
    );


    GPIO_SetPullCtl(
        PB,
        BIT4 | BIT5,
        GPIO_PUSEL_PULL_UP
    );


    /*
     * Prepare bus if SDA is currently low.
     */
    if(SDA == 0)
    {
        SCL = 0;

        delay(T2);
    }


    /*
     * START condition:
     *
     * while SCL is high,
     * SDA changes high -> low.
     */
    SDA = 1;
    delay(T2);

    SCL = 1;
    delay(T2);

    SDA = 0;
    delay(T2);

    SCL = 0;
    delay(T2);
}


/* =========================================================
   I2C STOP
   ========================================================= */

void i2c_stop(void)
{
    SDA = 0;
    delay(T2);

    SCL = 1;
    delay(T2);

    SDA = 1;
    delay(T2);
}


/* =========================================================
   I2C Write One Byte

   Return:
       0 = ACK
       1 = NACK
   ========================================================= */

uint8_t i2c_write(uint8_t d)
{
    TPU8 r;


    r.v = d;


    SDA = r.b7;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b6;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b5;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b4;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b3;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b2;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b1;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    SDA = r.b0;
    delay(T01);
    SCL = 1;
    delay(T0);
    SCL = 0;


    /* =========================
       ACK
       ========================= */

    /*
     * Release SDA.
     */
    SDA = 1;

    delay(T1);

    SCL = 1;

    delay(T1);


    /*
     * Slave:
     *     SDA = 0 -> ACK
     *     SDA = 1 -> NACK
     */
    r.v = SDA;


    SCL = 0;


    return r.v;
}


/* =========================================================
   I2C Read One Byte

   ack:
       0 = master sends ACK
       1 = master sends NACK
   ========================================================= */

uint8_t i2c_read(uint8_t ack)
{
    TPU8 r;


    /*
     * Release SDA so the slave can drive it.
     */
    SDA = 1;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b7 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b6 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b5 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b4 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b3 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b2 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b1 = SDA;
    SCL = 0;


    delay(T0);
    SCL = 1;
    delay(T0);
    r.b0 = SDA;
    SCL = 0;


    /* =========================
       Master ACK / NACK
       ========================= */

    SDA = ack;

    delay(T1);

    SCL = 1;

    delay(T1);

    SCL = 0;


    SDA = 1;


    return r.v;
}


/* =========================================================
   I2C Scan

   This function protects its whole scan with the mutex.
   ========================================================= */

uint16_t i2c_scan(uint8_t n)
{
    uint16_t i;
    uint16_t r;


    if(!i2c_lock())
    {
        return 0xFFFF;
    }


    n &= 0xFE;


    for(i = n; i < 255; i += 2)
    {
        i2c_start();

        r =
            i2c_write(
                (uint8_t)i
            );


        i2c_stop();


        if(r == 0)
        {
            i2c_unlock();

            return i;
        }
    }


    i2c_unlock();


    return 0xFFFF;
}
