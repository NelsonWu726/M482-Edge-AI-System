#include <stdint.h>
#include "NuMicro.h"


/* =========================================================
   myi2c.c
   ========================================================= */

void i2c_start(void);
void i2c_stop(void);

uint8_t i2c_write(uint8_t d);
uint8_t i2c_read(uint8_t ack);

uint8_t i2c_lock(void);
void i2c_unlock(void);


/* =========================================================
   I2C ACK / NACK
   ========================================================= */

#define I2C_ACK     0
#define I2C_NACK    1


/* =========================================================
   MAX30102 I2C Address

   7-bit slave address = 0x57

   Write:
       0x57 << 1 = 0xAE

   Read:
       0xAE | 1 = 0xAF
   ========================================================= */

#define MAX30102_ADDR_WRITE     0xAE
#define MAX30102_ADDR_READ      0xAF


/* =========================================================
   MAX30102 Register Address
   ========================================================= */

/* FIFO */
#define MAX30102_REG_FIFO_WR_PTR        0x04
#define MAX30102_REG_OVF_COUNTER        0x05
#define MAX30102_REG_FIFO_RD_PTR        0x06
#define MAX30102_REG_FIFO_DATA          0x07
#define MAX30102_REG_FIFO_CONFIG        0x08

/* Configuration */
#define MAX30102_REG_MODE_CONFIG        0x09
#define MAX30102_REG_SPO2_CONFIG        0x0A

/* LED */
#define MAX30102_REG_LED1_PA            0x0C
#define MAX30102_REG_LED2_PA            0x0D

/* Device ID */
#define MAX30102_REG_REV_ID             0xFE
#define MAX30102_REG_PART_ID            0xFF


/* =========================================================
   MAX30102 Constants
   ========================================================= */

#define MAX30102_PART_ID_VALUE          0x15

#define MAX30102_RESET                  0x40
#define MAX30102_SHUTDOWN               0x80

#define MAX30102_MODE_SPO2              0x03
#define MAX30102_MODE_SPO2_SHUTDOWN     (MAX30102_SHUTDOWN | MAX30102_MODE_SPO2)


/* =========================================================
   Function Prototypes
   ========================================================= */

uint8_t max30102_read_reg(uint8_t reg);

uint8_t max30102_write_reg(
    uint8_t reg,
    uint8_t value
);

uint8_t max30102_check_id(void);

uint8_t max30102_init(void);

uint8_t max30102_start(void);

uint8_t max30102_stop(void);

uint8_t max30102_read_fifo(
    uint32_t *red,
    uint32_t *ir
);


/* =========================================================
   Read One Register
   ========================================================= */

uint8_t max30102_read_reg(uint8_t reg)
{
    uint8_t data;


    /*
     * Protect the complete register-read transaction.
     */
    if(!i2c_lock())
    {
        return 0xFF;
    }


    i2c_start();

    i2c_write(
        MAX30102_ADDR_WRITE
    );

    i2c_write(
        reg
    );


    /*
     * Repeated START
     */
    i2c_start();

    i2c_write(
        MAX30102_ADDR_READ
    );


    /*
     * Read one byte.
     * This is the final byte, so send NACK.
     */
    data =
        i2c_read(
            I2C_NACK
        );


    i2c_stop();

    i2c_unlock();


    return data;
}


/* =========================================================
   Write One Register
   ========================================================= */

uint8_t max30102_write_reg(
    uint8_t reg,
    uint8_t value
)
{
    uint8_t ack;


    /*
     * Protect the complete register-write transaction.
     */
    if(!i2c_lock())
    {
        return 0;
    }


    i2c_start();


    /* MAX30102 + Write */
    ack = i2c_write(MAX30102_ADDR_WRITE);

    if(ack != 0)
    {
        i2c_stop();

        i2c_unlock();

        return 0;
    }


    /* Register address */
    ack = i2c_write(reg);

    if(ack != 0)
    {
        i2c_stop();

        i2c_unlock();

        return 0;
    }


    /* Register data */
    ack = i2c_write(value);

    if(ack != 0)
    {
        i2c_stop();

        i2c_unlock();

        return 0;
    }


    i2c_stop();


    i2c_unlock();


    return 1;
}


/* =========================================================
   Check MAX30102 Part ID

   PART_ID register = 0xFF
   Expected value   = 0x15
   ========================================================= */

uint8_t max30102_check_id(void)
{
    uint8_t id;

    id = max30102_read_reg(MAX30102_REG_PART_ID);

    if(id == MAX30102_PART_ID_VALUE)
    {
        return 1;
    }

    return 0;
}


/* =========================================================
   MAX30102 Initialization

   Important:
       init() only configures the sensor.

       At the end of init(), the MAX30102 is placed in
       shutdown mode.

       Actual measurement starts only when
       max30102_start() is called by max30102_task.
   ========================================================= */

uint8_t max30102_init(void)
{
    uint32_t timeout;

    /* =====================================================
       1. Check Device
       ===================================================== */

    if(!max30102_check_id())
    {
        return 0;
    }


    /* =====================================================
       2. Reset MAX30102
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_MODE_CONFIG,
            MAX30102_RESET
        )
    )
    {
        return 0;
    }


    /*
     * RESET bit clears automatically after reset.
     */
    timeout = 100;

    while(
        max30102_read_reg(
            MAX30102_REG_MODE_CONFIG
        ) & MAX30102_RESET
    )
    {
        if(timeout == 0)
        {
            return 0;
        }

        timeout--;
    }


    /* =====================================================
       3. FIFO Configuration

       0x5F = 0101 1111

       SMP_AVE = 010
               = average 4 samples

       FIFO_ROLLOVER_EN = 1

       FIFO_A_FULL = 1111
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_FIFO_CONFIG,
            0x5F
        )
    )
    {
        return 0;
    }


    /* =====================================================
       4. SpO2 Configuration

       0x6F = 0110 1111

       ADC range:
           11 = 16384 nA

       Sample rate:
           011 = 400 samples/sec

       LED pulse width:
           11 = 411 us
              = 18-bit ADC

       FIFO averaging = 4
       -> about 100 FIFO samples/sec
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_SPO2_CONFIG,
            0x6F
        )
    )
    {
        return 0;
    }


    /* =====================================================
       5. LED Current

       LED1 = RED
       LED2 = IR
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_LED1_PA,
            0x24
        )
    )
    {
        return 0;
    }


    if(
        !max30102_write_reg(
            MAX30102_REG_LED2_PA,
            0x2C
        )
    )
    {
        return 0;
    }


    /* =====================================================
       6. Clear FIFO
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_FIFO_WR_PTR,
            0x00
        )
    )
    {
        return 0;
    }


    if(
        !max30102_write_reg(
            MAX30102_REG_OVF_COUNTER,
            0x00
        )
    )
    {
        return 0;
    }


    if(
        !max30102_write_reg(
            MAX30102_REG_FIFO_RD_PTR,
            0x00
        )
    )
    {
        return 0;
    }


    /* =====================================================
       7. Stay Stopped After Initialization

       0x83 = 1000 0011

       SHDN = 1
       MODE = 011 (SpO2)

       Sensor configuration is kept,
       but RED/IR measurement is stopped.
       ===================================================== */

    if(
        !max30102_write_reg(
            MAX30102_REG_MODE_CONFIG,
            MAX30102_MODE_SPO2_SHUTDOWN
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Start MAX30102 Measurement

   Called when AI detects a person.
   ========================================================= */

uint8_t max30102_start(void)
{
    /*
     * Clear old FIFO data before a new measurement session.
     */

    if(
        !max30102_write_reg(
            MAX30102_REG_FIFO_WR_PTR,
            0x00
        )
    )
    {
        return 0;
    }


    if(
        !max30102_write_reg(
            MAX30102_REG_OVF_COUNTER,
            0x00
        )
    )
    {
        return 0;
    }


    if(
        !max30102_write_reg(
            MAX30102_REG_FIFO_RD_PTR,
            0x00
        )
    )
    {
        return 0;
    }


    /*
     * SHDN = 0
     * MODE = 011
     *
     * Start RED + IR SpO2 measurement.
     */

    if(
        !max30102_write_reg(
            MAX30102_REG_MODE_CONFIG,
            MAX30102_MODE_SPO2
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Stop MAX30102 Measurement

   Called when AI detects that the person is gone.
   ========================================================= */

uint8_t max30102_stop(void)
{
    /*
     * SHDN = 1
     * MODE = 011
     *
     * Enter power-save / shutdown mode.
     */

    if(
        !max30102_write_reg(
            MAX30102_REG_MODE_CONFIG,
            MAX30102_MODE_SPO2_SHUTDOWN
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Read One RED + IR Sample From FIFO

   Return:
       1 = new sample read
       0 = FIFO empty / invalid pointer

   Output:
       *red = RED ADC value
       *ir  = IR ADC value
   ========================================================= */

uint8_t max30102_read_fifo(
    uint32_t *red,
    uint32_t *ir
)
{
    uint8_t wr_ptr;
    uint8_t rd_ptr;

    uint8_t b1;
    uint8_t b2;
    uint8_t b3;


    if(
        (red == 0) ||
        (ir == 0)
    )
    {
        return 0;
    }


    /* =====================================================
       Check FIFO
       ===================================================== */

    wr_ptr =
        max30102_read_reg(
            MAX30102_REG_FIFO_WR_PTR
        );


    rd_ptr =
        max30102_read_reg(
            MAX30102_REG_FIFO_RD_PTR
        );


    if(wr_ptr == rd_ptr)
    {
        return 0;
    }


    /* =====================================================
       Select FIFO_DATA Register

       Protect this complete FIFO transaction so BH1750
       cannot use PB4/PB5 in the middle of it.
       ===================================================== */

    if(!i2c_lock())
    {
        return 0;
    }


    i2c_start();

    i2c_write(MAX30102_ADDR_WRITE);

    i2c_write(MAX30102_REG_FIFO_DATA);

    /* Repeated START */
    i2c_start();

    i2c_write(MAX30102_ADDR_READ);


    /* =====================================================
       RED: 3 bytes
       ===================================================== */

    b1 = i2c_read(I2C_ACK);
    b2 = i2c_read(I2C_ACK);
    b3 = i2c_read(I2C_ACK);

    *red =
        ((uint32_t)b1 << 16) |
        ((uint32_t)b2 << 8)  |
        ((uint32_t)b3);


    /* =====================================================
       IR: 3 bytes
       ===================================================== */

    b1 = i2c_read(I2C_ACK);
    b2 = i2c_read(I2C_ACK);
    b3 = i2c_read(I2C_NACK);

    *ir =
        ((uint32_t)b1 << 16) |
        ((uint32_t)b2 << 8)  |
        ((uint32_t)b3);


    i2c_stop();


    i2c_unlock();


    /*
     * ADC data is 18-bit.
     * Keep only the lower 18 bits.
     */

    *red &= 0x03FFFF;
    *ir  &= 0x03FFFF;


    return 1;
}
