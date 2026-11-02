#include "NuMicro.h"
#include "cmsis_os.h"


/* =========================================================
   myi2c.c
   ========================================================= */

uint8_t i2c_lock(void);
void i2c_unlock(void);

void i2c_start(void);
void i2c_stop(void);

uint8_t i2c_write(uint8_t d);
uint8_t i2c_read(uint8_t ack);


/* =========================================================
   BH1750 Init
   ========================================================= */

void bh1750_init(void)
{
    /*
     * Lock the shared PB4/PB5 I2C bus for the whole
     * BH1750 transaction.
     */
    if(!i2c_lock())
    {
        return;
    }


    i2c_start();


    /*
     * BH1750 + Write
     */
    i2c_write(
        0x46
    );


    /*
     * Continuous H-Resolution Mode2
     */
    i2c_write(
        0x11
    );


    i2c_stop();


    /*
     * Release I2C immediately.
     *
     * Do NOT keep the mutex during the 180 ms sensor
     * conversion delay.
     */
    i2c_unlock();


    osDelay(180);
}


/* =========================================================
   Read Light
   ========================================================= */

uint16_t bh1750_get_light(void)
{
    uint16_t ans;


    /*
     * MAX30102 may also be using PB4/PB5.
     * Wait until the complete shared I2C bus is free.
     */
    if(!i2c_lock())
    {
        return 0;
    }


    i2c_start();


    /*
     * BH1750 + Read
     */
    i2c_write(
        0x47
    );


    /*
     * High byte
     */
    ans =
        ((uint16_t)i2c_read(0))
        << 8;


    /*
     * Low byte
     */
    ans +=
        i2c_read(1);


    i2c_stop();


    /*
     * Complete transaction finished.
     */
    i2c_unlock();


    return ans;
}
