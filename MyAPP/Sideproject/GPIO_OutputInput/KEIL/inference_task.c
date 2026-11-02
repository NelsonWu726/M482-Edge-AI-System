#include <stdint.h>
#include "cmsis_os.h"


/* =========================================================
   AI Configuration
   ========================================================= */

#define AI_WINDOW_SIZE       10
#define AI_FEATURE_COUNT      2


/* =========================================================
   AI Result States
   ========================================================= */

#define AI_RESULT_EMPTY       0
#define AI_RESULT_OCCUPIED    1
#define AI_RESULT_WARMUP      2


/* =========================================================
   MAX30102 Signals
   ========================================================= */

#define MAX30102_SIGNAL_START    0x01
#define MAX30102_SIGNAL_STOP     0x02


/* =========================================================
   Network Message Configuration

   Must be identical to app_queue.c / max30102_task.c /
   network_task.c because this project currently does not use
   a shared header file.
   ========================================================= */

#define NETWORK_MSG_ENV         1
#define NETWORK_MSG_PPG         2
#define NETWORK_MSG_HEART       3

#define PPG_BATCH_SIZE         10


/* =========================================================
   Shared Data Types
   ========================================================= */

typedef struct
{
    float temperature;
    uint16_t lux;

} sensor_data_t;


typedef struct
{
    float temperature;
    uint16_t lux;
    uint8_t ai_result;

} inference_result_t;


typedef struct
{
    /* =========================
       Common
       ========================= */

    uint8_t type;


    /* =========================
       ENV
       ========================= */

    float temperature;

    uint16_t lux;

    uint8_t ai_result;


    /* =========================
       PPG
       ========================= */

    uint8_t ppg_count;

    uint32_t red[PPG_BATCH_SIZE];

    uint32_t ir[PPG_BATCH_SIZE];


    /* =========================
       HEART
       ========================= */

    uint16_t bpm;

    uint8_t bpm_valid;

    uint8_t spo2;

    uint8_t spo2_valid;

    uint8_t finger_present;

} network_message_t;


/* =========================================================
   app_queue.c
   ========================================================= */

extern osMailQId sensor_queue_id;

extern osMailQId result_queue_id;

extern osMailQId network_queue_id;


/* =========================================================
   app.c
   ========================================================= */

extern osThreadId max30102_task_id;


/* =========================================================
   ai_model.cpp
   ========================================================= */

extern int ai_model_is_ready(void);

extern int ai_model_predict(
    const float *input_20,
    uint8_t *ai_result
);


/* =========================================================
   AI Rolling Window
   ========================================================= */

static float g_ai_window
[
    AI_WINDOW_SIZE
]
[
    AI_FEATURE_COUNT
];


static uint8_t g_ai_sample_count = 0;


/* =========================================================
   Add Sensor Sample
   ========================================================= */

static uint8_t ai_window_add_sample(
    float temperature,
    uint16_t lux
)
{
    int i;


    /* =====================================================
       Initial Warm-up
       ===================================================== */

    if(g_ai_sample_count < AI_WINDOW_SIZE)
    {
        g_ai_window
        [
            g_ai_sample_count
        ]
        [0] =
            temperature;


        g_ai_window
        [
            g_ai_sample_count
        ]
        [1] =
            (float)lux;


        g_ai_sample_count++;


        if(g_ai_sample_count < AI_WINDOW_SIZE)
        {
            return 0;
        }


        return 1;
    }


    /* =====================================================
       Sliding Window
       ===================================================== */

    for(i = 0; i < AI_WINDOW_SIZE - 1; i++)
    {
        g_ai_window[i][0] =
            g_ai_window[i + 1][0];

        g_ai_window[i][1] =
            g_ai_window[i + 1][1];
    }


    /* =========================
       Insert Newest Sample
       ========================= */

    g_ai_window
    [
        AI_WINDOW_SIZE - 1
    ]
    [0] =
        temperature;


    g_ai_window
    [
        AI_WINDOW_SIZE - 1
    ]
    [1] =
        (float)lux;


    return 1;
}


/* =========================================================
   Inference Task
   ========================================================= */

void inference_task(
    void const *argument
)
{
    osEvent event;

    sensor_data_t *data;

    inference_result_t *output_result;

    network_message_t *network_result;

    uint8_t window_ready;
    uint8_t ai_result;

    /*
     * Last valid AI state.
     *
     * WARMUP means that no valid EMPTY / OCCUPIED state
     * has been established yet.
     */
    uint8_t previous_ai_result =
        AI_RESULT_WARMUP;

    int ai_status;


    (void)argument;


    for(;;)
    {
        /* =================================================
           Wait For Sensor Data
           ================================================= */

        event =
            osMailGet(
                sensor_queue_id,
                osWaitForever
            );


        if(event.status != osEventMail)
        {
            continue;
        }


        data =
            (sensor_data_t *)
            event.value.p;


        /* =================================================
           Add Sample To AI Window
           ================================================= */

        window_ready =
            ai_window_add_sample(
                data->temperature,
                data->lux
            );


        /* =================================================
           Default AI State
           ================================================= */

        ai_result =
            AI_RESULT_WARMUP;


        /* =================================================
           TFLite Micro Inference
           ================================================= */

        if(
            (window_ready != 0) &&
            (ai_model_is_ready() != 0)
        )
        {
            ai_status =
                ai_model_predict(
                    &g_ai_window[0][0],
                    &ai_result
                );


            if(ai_status != 0)
            {
                ai_result =
                    AI_RESULT_WARMUP;
            }
        }


        /* =================================================
           Control MAX30102

           Only send a signal when the VALID AI state changes.

           WARMUP -> OCCUPIED:
               START

           EMPTY -> OCCUPIED:
               START

           OCCUPIED -> EMPTY:
               STOP

           Repeated OCCUPIED / EMPTY:
               do nothing

           WARMUP caused by inference failure:
               do not change MAX30102 state.
           ================================================= */

        if(
            (ai_result != AI_RESULT_WARMUP) &&
            (ai_result != previous_ai_result)
        )
        {
            if(max30102_task_id != NULL)
            {
                if(ai_result == AI_RESULT_OCCUPIED)
                {
                    osSignalSet(
                        max30102_task_id,
                        MAX30102_SIGNAL_START
                    );
                }
                else if(
                    (ai_result == AI_RESULT_EMPTY) &&
                    (previous_ai_result == AI_RESULT_OCCUPIED)
                )
                {
                    osSignalSet(
                        max30102_task_id,
                        MAX30102_SIGNAL_STOP
                    );
                }


                previous_ai_result =
                    ai_result;
            }
        }


        /* =================================================
           1. Output Queue

           This queue still uses inference_result_t because
           output_task only needs Temp / Lux / AI.
           ================================================= */

        output_result =
            (inference_result_t *)
            osMailAlloc(
                result_queue_id,
                osWaitForever
            );


        if(output_result != NULL)
        {
            output_result->temperature =
                data->temperature;

            output_result->lux =
                data->lux;

            output_result->ai_result =
                ai_result;


            if(
                osMailPut(
                    result_queue_id,
                    output_result
                ) != osOK
            )
            {
                osMailFree(
                    result_queue_id,
                    output_result
                );
            }
        }


        /* =================================================
           2. Network Queue

           Send ENV message:
               temperature
               lux
               AI state

           timeout = 0:
           network failure must NOT block AI inference.
           ================================================= */

        network_result =
            (network_message_t *)
            osMailAlloc(
                network_queue_id,
                0
            );


        if(network_result != NULL)
        {
            /* =========================
               ENV Message
               ========================= */

            network_result->type =
                NETWORK_MSG_ENV;


            network_result->temperature =
                data->temperature;


            network_result->lux =
                data->lux;


            network_result->ai_result =
                ai_result;


            /* =========================
               PPG Fields Not Used
               ========================= */

            network_result->ppg_count =
                0;


            /* =========================
               HEART Fields Not Used
               ========================= */

            network_result->bpm =
                0;


            network_result->bpm_valid =
                0;


            network_result->spo2 =
                0;


            network_result->spo2_valid =
                0;


            network_result->finger_present =
                0;


            /* =========================
               Send To Network Queue
               ========================= */

            if(
                osMailPut(
                    network_queue_id,
                    network_result
                ) != osOK
            )
            {
                osMailFree(
                    network_queue_id,
                    network_result
                );
            }
        }


        /* =================================================
           Release Sensor Mail
           ================================================= */

        osMailFree(
            sensor_queue_id,
            data
        );
    }
}
