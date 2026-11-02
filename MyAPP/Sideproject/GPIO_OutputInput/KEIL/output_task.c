#include <stdio.h>
#include <stdint.h>
#include "cmsis_os.h"


/* =========================================================
   lcd1602.c
   ========================================================= */

void lcd_print(uint8_t pos, char *p);


/* =========================================================
   buzzer.c
   ========================================================= */

void init_buzzer(void);
void buzzer_on(uint16_t ms);


/* =========================================================
   timer1.c
   ========================================================= */

void init_TIMER0(void);


/* =========================================================
   Shared Inference Result Type
   ========================================================= */

typedef struct
{
    float temperature;
    uint16_t lux;
    uint8_t ai_result;

} inference_result_t;


/* =========================================================
   app_queue.c
   ========================================================= */

extern osMailQId result_queue_id;


/* =========================================================
   Output Task
   ========================================================= */

void output_task(
    void const *argument
)
{
    osEvent event;

    inference_result_t *result;

    char line[17];

    int temp10;
    int whole;
    int decimal;

    /*
     * AI state:
     *
     * 0 = EMPTY
     * 1 = OCCUPIED
     * 2 = WARMUP
     */
    uint8_t last_ai_result = 2;

    /*
     * Only used for LCD warm-up display.
     */
    uint8_t warmup_count = 0;


    (void)argument;


    /* =====================================================
       Hardware Init
       ===================================================== */

    init_buzzer();

    init_TIMER0();


    for(;;)
    {
        /* =================================================
           Wait Inference Result
           ================================================= */

        event =
            osMailGet(
                result_queue_id,
                osWaitForever
            );


        if(event.status != osEventMail)
        {
            continue;
        }


        result =
            (inference_result_t *)
            event.value.p;


        /* =================================================
           Temperature Format
           ================================================= */

        temp10 =
            (int)(
                result->temperature *
                10.0f
            );


        whole =
            temp10 / 10;


        decimal =
            temp10 % 10;


        /*
         * Avoid negative decimal digit
         * such as -25.-3
         */
        if(decimal < 0)
        {
            decimal = -decimal;
        }


        /* =================================================
           LCD Line 1
           ================================================= */

        snprintf(
            line,
            sizeof(line),
            "T:%d.%d L:%u",
            whole,
            decimal,
            result->lux
        );


        lcd_print(
            0x00,
            line
        );


        /* =================================================
           LCD Line 2 - AI State
           ================================================= */

        if(result->ai_result == 2)
        {
            /* =============================================
               Warm-up
               ============================================= */

            if(warmup_count < 9)
            {
                warmup_count++;
            }


            snprintf(
                line,
                sizeof(line),
                "AI WAIT %u/10   ",
                warmup_count
            );


            lcd_print(
                0x40,
                line
            );
        }
        else if(result->ai_result == 1)
        {
            /* =============================================
               OCCUPIED
               ============================================= */

            lcd_print(
                0x40,
                "AI: OCCUPIED    "
            );


            /*
             * Buzz only when entering OCCUPIED.
             *
             * EMPTY -> OCCUPIED
             * WARMUP -> OCCUPIED
             */
            if(last_ai_result != 1)
            {
                buzzer_on(500);
            }
        }
        else
        {
            /* =============================================
               EMPTY
               ============================================= */

            lcd_print(
                0x40,
                "AI: EMPTY       "
            );
        }


        /* =================================================
           Remember State
           ================================================= */

        last_ai_result =
            result->ai_result;


        /* =================================================
           Release Result Mail
           ================================================= */

        osMailFree(
            result_queue_id,
            result
        );
    }
}