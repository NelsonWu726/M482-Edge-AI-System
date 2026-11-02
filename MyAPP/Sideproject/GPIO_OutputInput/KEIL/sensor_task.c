#include <stdint.h>
#include "cmsis_os.h"


/* =========================================================
   adc.c
   ========================================================= */

void init_adc(void);
void adc_start(void);
uint8_t adc_is_ready(void);
float adc_get_temperature(void);


/* =========================================================
   bh1750.c
   ========================================================= */

void bh1750_init(void);
uint16_t bh1750_get_light(void);


/* =========================================================
   Shared Sensor Data Type
   ========================================================= */

typedef struct
{
    float temperature;
    uint16_t lux;

} sensor_data_t;


/* =========================================================
   app_queue.c
   ========================================================= */

extern osMailQId sensor_queue_id;


/* =========================================================
   Sensor Task
   ========================================================= */

void sensor_task(
    void const *argument
)
{
    uint32_t timeout;
    sensor_data_t *data;

    (void)argument;


    /* =========================
       Sensor Initialization
       ========================= */

    init_adc();

    bh1750_init();


    for(;;)
    {
        /* =========================
           Allocate Sensor Mail
           ========================= */

        data =
            (sensor_data_t *)
            osMailAlloc(
                sensor_queue_id,
                osWaitForever
            );


        if(data != NULL)
        {
            /* =========================
               Temperature
               ========================= */

            adc_start();

            timeout = 100;


            while(!adc_is_ready())
            {
                if(timeout == 0)
                {
                    break;
                }

                timeout--;

                osDelay(1);
            }


            if(adc_is_ready())
            {
                data->temperature =
                    adc_get_temperature();
            }
            else
            {
                /*
                 * ADC failed.
                 */
                osMailFree(
                    sensor_queue_id,
                    data
                );

                /*
                 * Retry later.
                 */
                osDelay(30000);

                continue;
            }


            /* =========================
               Light
               ========================= */

            data->lux =
                bh1750_get_light();


            /* =========================
               Send Sensor Data
               ========================= */

            if(
                osMailPut(
                    sensor_queue_id,
                    data
                ) != osOK
            )
            {
                osMailFree(
                    sensor_queue_id,
                    data
                );
            }
        }


        /* =================================================
           Model Sampling Interval
           ================================================= */

        /*
         * Training / deployment cadence:
         *
         * one Temp + Lux sample
         * every 30 seconds.
         */

        osDelay(5000);
    }
}