#include <stdint.h>
#include "cmsis_os.h"


/* =========================================================
   Network Message Configuration

   IMPORTANT:
   Must eventually be identical to:

       inference_task.c
       max30102_task.c
       network_task.c
   ========================================================= */

#define NETWORK_MSG_ENV         1
#define NETWORK_MSG_PPG         2
#define NETWORK_MSG_HEART       3

#define PPG_BATCH_SIZE         10


/* =========================================================
   Queue Data Types
   ========================================================= */

/*
 * sensor_task -> inference_task
 */
typedef struct
{
    float temperature;
    uint16_t lux;

} sensor_data_t;


/*
 * inference_task -> output_task
 */
typedef struct
{
    float temperature;
    uint16_t lux;
    uint8_t ai_result;

} inference_result_t;


/*
 * Network message
 *
 * Producers:
 *
 * inference_task
 *     -> ENV
 *
 * max30102_task
 *     -> PPG
 *     -> HEART
 *
 * Consumer:
 *
 * network_task
 *
 * IMPORTANT:
 *
 * HEART fields are appended at the END.
 *
 * This keeps the existing ENV / PPG member offsets
 * unchanged while we migrate the other source files.
 */
typedef struct
{
    /* =========================
       Common Message Type
       ========================= */

    uint8_t type;


    /* =========================
       ENV Message
       ========================= */

    float temperature;

    uint16_t lux;

    uint8_t ai_result;


    /* =========================
       PPG Message
       ========================= */

    uint8_t ppg_count;

    uint32_t red[PPG_BATCH_SIZE];

    uint32_t ir[PPG_BATCH_SIZE];


    /* =========================
       HEART Message

       Used when:
           type = NETWORK_MSG_HEART
       ========================= */

    uint16_t bpm;

    uint8_t bpm_valid;

    uint8_t spo2;

    uint8_t spo2_valid;

    uint8_t finger_present;

} network_message_t;


/* =========================================================
   Mail Queue Definitions
   ========================================================= */

/*
 * sensor_task -> inference_task
 */
osMailQDef(
    sensor_queue,
    4,
    sensor_data_t
);


/*
 * inference_task -> output_task
 */
osMailQDef(
    result_queue,
    4,
    inference_result_t
);


/*
 * Network Queue
 *
 * Producers:
 *
 *     inference_task
 *         ENV
 *
 *     max30102_task
 *         PPG
 *         HEART
 *
 * Consumer:
 *
 *     network_task
 *
 * Keep depth = 8.
 *
 * HEART packet rate will be low, so there is
 * currently no reason to increase this and use
 * more RTOS memory.
 */
osMailQDef(
    network_queue,
    8,
    network_message_t
);


/* =========================================================
   Mail Queue IDs
   ========================================================= */

osMailQId sensor_queue_id = NULL;

osMailQId result_queue_id = NULL;

osMailQId network_queue_id = NULL;


/* =========================================================
   Queue Initialization

   Return:

       0  = success
      -1  = sensor_queue failed
      -2  = result_queue failed
      -3  = network_queue failed
   ========================================================= */

int app_queue_init(void)
{
    /* =========================
       Sensor Queue
       ========================= */

    sensor_queue_id =
        osMailCreate(
            osMailQ(sensor_queue),
            NULL
        );


    if(sensor_queue_id == NULL)
    {
        return -1;
    }


    /* =========================
       Output Result Queue
       ========================= */

    result_queue_id =
        osMailCreate(
            osMailQ(result_queue),
            NULL
        );


    if(result_queue_id == NULL)
    {
        return -2;
    }


    /* =========================
       Network Queue
       ========================= */

    network_queue_id =
        osMailCreate(
            osMailQ(network_queue),
            NULL
        );


    if(network_queue_id == NULL)
    {
        return -3;
    }


    return 0;
}