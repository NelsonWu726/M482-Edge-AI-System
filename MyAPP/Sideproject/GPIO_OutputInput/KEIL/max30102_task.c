#include <stdint.h>
#include "cmsis_os.h"


/* =========================================================
   max30102.c APIs
   ========================================================= */

uint8_t max30102_init(void);
uint8_t max30102_start(void);
uint8_t max30102_stop(void);

uint8_t max30102_read_fifo(
    uint32_t *red,
    uint32_t *ir
);


/* =========================================================
   peak.c APIs
   ========================================================= */

void peak_init(uint32_t level);
uint32_t peak(uint32_t py);


/* =========================================================
   spo2_algorithm.c APIs
   ========================================================= */

void spo2_init(void);
void spo2_reset(void);

uint8_t spo2_update(
    uint32_t red,
    uint32_t ir
);

uint8_t spo2_get_value(
    uint32_t *spo2
);

uint8_t spo2_is_valid(void);


/* =========================================================
   MAX30102 Signals
   ========================================================= */

#define MAX30102_SIGNAL_START    0x01
#define MAX30102_SIGNAL_STOP     0x02


/* =========================================================
   Network Message Configuration

   IMPORTANT:
   Must stay IDENTICAL to:
       app_queue.c
       inference_task.c
       network_task.c
   ========================================================= */

#define NETWORK_MSG_ENV         1
#define NETWORK_MSG_PPG         2
#define NETWORK_MSG_HEART       3

#define PPG_BATCH_SIZE         10


/* =========================================================
   PPG Network Rate
   ========================================================= */

#define PPG_DECIMATION          5


/* =========================================================
   HEART Network Rate

   Short HEART packets are allowed at most about once/sec
   during normal updates.

   Finger insertion/removal and AI STOP use force-send.
   ========================================================= */

#define HEART_MIN_SEND_SAMPLES  100UL


/* =========================================================
   Finger Detection Configuration
   ========================================================= */

#define FINGER_ON_IR_THRESHOLD       5000UL
#define FINGER_OFF_IR_THRESHOLD      3000UL
#define FINGER_CONFIRM_SAMPLES         10


/* =========================================================
   BPM Configuration
   ========================================================= */

#define BPM_SAMPLE_RATE_HZ          100UL
#define BPM_HISTORY_BEATS             5UL

#define BPM_NUMERATOR \
    (60UL * BPM_SAMPLE_RATE_HZ * BPM_HISTORY_BEATS)

#define BPM_MIN_VALUE                40UL
#define BPM_MAX_VALUE               200UL
#define BPM_PEAK_LEVEL              200UL

#define BPM_TIMEOUT_SAMPLES \
    (BPM_SAMPLE_RATE_HZ * 5UL)


/* =========================================================
   Shared Network Message Type

   IMPORTANT:
   Field order must be identical in all 4 files.
   ========================================================= */

typedef struct
{
    uint8_t type;

    float temperature;
    uint16_t lux;
    uint8_t ai_result;

    uint8_t ppg_count;

    uint32_t red[PPG_BATCH_SIZE];
    uint32_t ir[PPG_BATCH_SIZE];

    uint16_t bpm;
    uint8_t bpm_valid;

    uint8_t spo2;
    uint8_t spo2_valid;

    uint8_t finger_present;

} network_message_t;


/* =========================================================
   app_queue.c
   ========================================================= */

extern osMailQId network_queue_id;


/* =========================================================
   PPG Batch Buffer
   ========================================================= */

static uint32_t g_red_batch[PPG_BATCH_SIZE];
static uint32_t g_ir_batch[PPG_BATCH_SIZE];


/* =========================================================
   HEART Send State
   ========================================================= */

static uint32_t g_last_heart_send_sample = 0;


/* =========================================================
   Debug Variables
   ========================================================= */

volatile uint8_t max30102_init_ok = 0;
volatile uint8_t max30102_measuring_debug = 0;

volatile uint32_t max30102_last_red = 0;
volatile uint32_t max30102_last_ir = 0;

volatile uint32_t max30102_sample_count = 0;
volatile uint32_t max30102_network_sample_count = 0;

volatile uint32_t max30102_ppg_queue_ok = 0;
volatile uint32_t max30102_ppg_queue_drop = 0;

volatile uint32_t max30102_heart_queue_ok = 0;
volatile uint32_t max30102_heart_queue_drop = 0;


/* =========================================================
   Finger Debug
   ========================================================= */

volatile uint8_t max30102_finger_present = 0;

volatile uint8_t max30102_finger_on_count = 0;
volatile uint8_t max30102_finger_off_count = 0;

volatile uint32_t max30102_finger_on_events = 0;
volatile uint32_t max30102_finger_off_events = 0;


/* =========================================================
   BPM Debug
   ========================================================= */

volatile uint32_t max30102_bpm = 0;
volatile uint8_t max30102_bpm_valid = 0;

volatile uint32_t max30102_peak_distance = 0;

volatile uint32_t max30102_bpm_update_count = 0;
volatile uint32_t max30102_bpm_reject_count = 0;

volatile uint32_t max30102_bpm_age_samples = 0;


/* =========================================================
   SpO2 Debug
   ========================================================= */

volatile uint32_t max30102_spo2 = 0;
volatile uint8_t max30102_spo2_valid = 0;

volatile uint32_t max30102_spo2_update_count = 0;


/* =========================================================
   Reset BPM
   ========================================================= */

static void max30102_bpm_reset(void)
{
    peak_init(
        BPM_PEAK_LEVEL
    );

    max30102_bpm = 0;
    max30102_bpm_valid = 0;

    max30102_peak_distance = 0;
    max30102_bpm_age_samples = 0;
}


/* =========================================================
   Reset SpO2
   ========================================================= */

static void max30102_spo2_reset(void)
{
    spo2_reset();

    max30102_spo2 = 0;
    max30102_spo2_valid = 0;
}


/* =========================================================
   Reset Finger + Heart Measurement State
   ========================================================= */

static void max30102_finger_reset(void)
{
    max30102_finger_present = 0;

    max30102_finger_on_count = 0;
    max30102_finger_off_count = 0;

    max30102_bpm_reset();
    max30102_spo2_reset();
}


/* =========================================================
   Finger Detection
   ========================================================= */

static uint8_t max30102_update_finger(
    uint32_t ir
)
{
    if(!max30102_finger_present)
    {
        if(ir >= FINGER_ON_IR_THRESHOLD)
        {
            if(
                max30102_finger_on_count <
                FINGER_CONFIRM_SAMPLES
            )
            {
                max30102_finger_on_count++;
            }

            if(
                max30102_finger_on_count >=
                FINGER_CONFIRM_SAMPLES
            )
            {
                max30102_finger_present = 1;

                max30102_finger_on_count = 0;
                max30102_finger_off_count = 0;

                max30102_finger_on_events++;

                /*
                 * New finger:
                 * start BPM + SpO2 from clean state.
                 */
                max30102_bpm_reset();
                max30102_spo2_reset();
            }
        }
        else
        {
            max30102_finger_on_count = 0;
        }
    }
    else
    {
        if(ir <= FINGER_OFF_IR_THRESHOLD)
        {
            if(
                max30102_finger_off_count <
                FINGER_CONFIRM_SAMPLES
            )
            {
                max30102_finger_off_count++;
            }

            if(
                max30102_finger_off_count >=
                FINGER_CONFIRM_SAMPLES
            )
            {
                max30102_finger_present = 0;

                max30102_finger_on_count = 0;
                max30102_finger_off_count = 0;

                max30102_finger_off_events++;

                /*
                 * Finger removed:
                 * immediately invalidate old results.
                 */
                max30102_bpm_reset();
                max30102_spo2_reset();
            }
        }
        else
        {
            max30102_finger_off_count = 0;
        }
    }

    return max30102_finger_present;
}


/* =========================================================
   Process One RAW IR Sample For BPM

   Return:
       1 = BPM status/value changed
       0 = no externally visible change
   ========================================================= */

static uint8_t max30102_process_bpm(
    uint32_t ir
)
{
    uint32_t dd;
    uint32_t bpm_candidate;

    uint32_t old_bpm;
    uint8_t old_valid;


    old_bpm = max30102_bpm;
    old_valid = max30102_bpm_valid;


    dd =
        peak(
            ir
        );


    /* Age latest valid BPM. */
    if(max30102_bpm_valid)
    {
        if(
            max30102_bpm_age_samples <
            0xFFFFFFFFUL
        )
        {
            max30102_bpm_age_samples++;
        }

        if(
            max30102_bpm_age_samples >
            BPM_TIMEOUT_SAMPLES
        )
        {
            max30102_bpm = 0;
            max30102_bpm_valid = 0;
        }
    }


    if(dd != 0)
    {
        max30102_peak_distance =
            dd;


        bpm_candidate =
            BPM_NUMERATOR /
            dd;


        if(
            (bpm_candidate >= BPM_MIN_VALUE) &&
            (bpm_candidate <= BPM_MAX_VALUE)
        )
        {
            max30102_bpm =
                bpm_candidate;

            max30102_bpm_valid =
                1;

            max30102_bpm_age_samples =
                0;

            max30102_bpm_update_count++;
        }
        else
        {
            max30102_bpm_reject_count++;
        }
    }


    if(
        (old_bpm != max30102_bpm) ||
        (old_valid != max30102_bpm_valid)
    )
    {
        return 1;
    }


    return 0;
}


/* =========================================================
   Process One RAW RED + IR Sample For SpO2

   Return:
       1 = SpO2 status/value changed or a new valid
           window was produced
       0 = no externally visible change
   ========================================================= */

static uint8_t max30102_process_spo2(
    uint32_t red,
    uint32_t ir
)
{
    uint32_t spo2_value;

    uint32_t old_spo2;
    uint8_t old_valid;

    uint8_t new_valid_result;


    old_spo2 =
        max30102_spo2;

    old_valid =
        max30102_spo2_valid;


    new_valid_result =
        spo2_update(
            red,
            ir
        );


    if(new_valid_result)
    {
        if(
            spo2_get_value(
                &spo2_value
            )
        )
        {
            max30102_spo2 =
                spo2_value;

            max30102_spo2_valid =
                1;

            max30102_spo2_update_count++;
        }
    }


    /*
     * A completed bad window may invalidate the algorithm.
     * Do not keep stale SpO2 marked as valid.
     */
    if(!spo2_is_valid())
    {
        max30102_spo2 = 0;
        max30102_spo2_valid = 0;
    }


    if(
        new_valid_result ||
        (old_spo2 != max30102_spo2) ||
        (old_valid != max30102_spo2_valid)
    )
    {
        return 1;
    }


    return 0;
}


/* =========================================================
   Send One HEART Status Message

   force = 0:
       rate limited to about 1 packet/sec

   force = 1:
       immediate state notification
       (finger inserted/removed or AI STOP)
   ========================================================= */

static void max30102_send_heart_status(
    uint8_t force
)
{
    network_message_t *msg;

    uint32_t elapsed;


    elapsed =
        max30102_sample_count -
        g_last_heart_send_sample;


    if(
        !force &&
        (elapsed < HEART_MIN_SEND_SAMPLES)
    )
    {
        return;
    }


    msg =
        (network_message_t *)
        osMailAlloc(
            network_queue_id,
            0
        );


    if(msg == NULL)
    {
        max30102_heart_queue_drop++;

        return;
    }


    msg->type =
        NETWORK_MSG_HEART;


    /* ENV fields not used. */
    msg->temperature = 0.0f;
    msg->lux = 0;
    msg->ai_result = 0;


    /* PPG fields not used. */
    msg->ppg_count = 0;


    /* HEART fields. */
    msg->bpm =
        (uint16_t)max30102_bpm;

    msg->bpm_valid =
        max30102_bpm_valid;

    msg->spo2 =
        (uint8_t)max30102_spo2;

    msg->spo2_valid =
        max30102_spo2_valid;

    msg->finger_present =
        max30102_finger_present;


    if(
        osMailPut(
            network_queue_id,
            msg
        ) != osOK
    )
    {
        osMailFree(
            network_queue_id,
            msg
        );

        max30102_heart_queue_drop++;

        return;
    }


    g_last_heart_send_sample =
        max30102_sample_count;


    max30102_heart_queue_ok++;
}


/* =========================================================
   Send One PPG Batch To Network Queue
   ========================================================= */

static void max30102_send_ppg_batch(
    uint8_t count
)
{
    network_message_t *msg;

    uint8_t i;


    if(count == 0)
    {
        return;
    }


    if(count > PPG_BATCH_SIZE)
    {
        count =
            PPG_BATCH_SIZE;
    }


    msg =
        (network_message_t *)
        osMailAlloc(
            network_queue_id,
            0
        );


    if(msg == NULL)
    {
        max30102_ppg_queue_drop++;

        return;
    }


    msg->type =
        NETWORK_MSG_PPG;


    /* ENV fields not used. */
    msg->temperature = 0.0f;
    msg->lux = 0;
    msg->ai_result = 0;


    msg->ppg_count =
        count;


    for(i = 0; i < count; i++)
    {
        msg->red[i] =
            g_red_batch[i];

        msg->ir[i] =
            g_ir_batch[i];
    }


    /* HEART fields not used. */
    msg->bpm = 0;
    msg->bpm_valid = 0;

    msg->spo2 = 0;
    msg->spo2_valid = 0;

    msg->finger_present = 0;


    if(
        osMailPut(
            network_queue_id,
            msg
        ) != osOK
    )
    {
        osMailFree(
            network_queue_id,
            msg
        );

        max30102_ppg_queue_drop++;

        return;
    }


    max30102_ppg_queue_ok++;
}


/* =========================================================
   MAX30102 Task
   ========================================================= */

void max30102_task(
    void const *argument
)
{
    uint32_t red;
    uint32_t ir;

    uint8_t measuring = 0;

    uint8_t batch_count = 0;
    uint8_t decimation_count = 0;

    uint8_t old_finger_state;
    uint8_t bpm_changed;
    uint8_t spo2_changed;

    osEvent event;


    (void)argument;


    /* =====================================================
       Initialize MAX30102
       ===================================================== */

    if(!max30102_init())
    {
        max30102_init_ok = 0;

        for(;;)
        {
            osDelay(1000);
        }
    }


    max30102_init_ok = 1;


    /*
     * Initialize SpO2 module once.
     */
    spo2_init();


    max30102_finger_reset();


    g_last_heart_send_sample =
        max30102_sample_count;


    /* =====================================================
       Main Loop
       ===================================================== */

    for(;;)
    {
        /* =================================================
           Not Measuring
           ================================================= */

        if(!measuring)
        {
            max30102_measuring_debug =
                0;


            event =
                osSignalWait(
                    0,
                    osWaitForever
                );


            if(event.status == osEventSignal)
            {
                /*
                 * STOP has priority.
                 */
                if(
                    event.value.signals &
                    MAX30102_SIGNAL_STOP
                )
                {
                    max30102_stop();

                    measuring = 0;

                    batch_count = 0;
                    decimation_count = 0;

                    max30102_finger_reset();

                    /*
                     * Clear HEART state on PC.
                     */
                    max30102_send_heart_status(
                        1
                    );
                }

                else if(
                    event.value.signals &
                    MAX30102_SIGNAL_START
                )
                {
                    if(max30102_start())
                    {
                        measuring = 1;

                        batch_count = 0;
                        decimation_count = 0;

                        max30102_finger_reset();

                        max30102_measuring_debug = 1;
                    }
                }
            }
        }


        /* =================================================
           Measuring
           ================================================= */

        while(measuring)
        {
            /* =============================================
               Check STOP Signal
               ============================================= */

            event =
                osSignalWait(
                    0,
                    0
                );


            if(event.status == osEventSignal)
            {
                if(
                    event.value.signals &
                    MAX30102_SIGNAL_STOP
                )
                {
                    max30102_stop();

                    measuring = 0;

                    max30102_measuring_debug = 0;

                    batch_count = 0;
                    decimation_count = 0;

                    max30102_finger_reset();

                    /*
                     * Immediately tell PC:
                     * finger=0, bpm_valid=0, spo2_valid=0.
                     */
                    max30102_send_heart_status(
                        1
                    );

                    break;
                }
            }


            /* =============================================
               Read One RAW RED + IR Sample
               ============================================= */

            if(
                max30102_read_fifo(
                    &red,
                    &ir
                )
            )
            {
                max30102_last_red =
                    red;

                max30102_last_ir =
                    ir;

                max30102_sample_count++;


                /* =========================================
                   Finger Detection
                   ========================================= */

                old_finger_state =
                    max30102_finger_present;


                max30102_update_finger(
                    ir
                );


                /*
                 * Finger inserted or removed:
                 * send state immediately.
                 */
                if(
                    old_finger_state !=
                    max30102_finger_present
                )
                {
                    max30102_send_heart_status(
                        1
                    );
                }


                /* =========================================
                   BPM + SpO2

                   Use RAW data before network decimation.
                   ========================================= */

                if(max30102_finger_present)
                {
                    bpm_changed =
                        max30102_process_bpm(
                            ir
                        );


                    spo2_changed =
                        max30102_process_spo2(
                            red,
                            ir
                        );


                    /*
                     * Send a short HEART packet when a
                     * result changes, rate-limited to
                     * about 1 packet/sec.
                     */
                    if(
                        bpm_changed ||
                        spo2_changed
                    )
                    {
                        max30102_send_heart_status(
                            0
                        );
                    }
                }


                /* =========================================
                   Network PPG

                   RAW ~100 Hz
                       ↓
                   every 5th sample
                       ↓
                   PC ~20 Hz
                   ========================================= */

                decimation_count++;


                if(
                    decimation_count >=
                    PPG_DECIMATION
                )
                {
                    decimation_count = 0;


                    g_red_batch[batch_count] =
                        red;

                    g_ir_batch[batch_count] =
                        ir;


                    batch_count++;

                    max30102_network_sample_count++;


                    if(
                        batch_count >=
                        PPG_BATCH_SIZE
                    )
                    {
                        max30102_send_ppg_batch(
                            batch_count
                        );

                        batch_count = 0;
                    }
                }
            }


            /*
             * FIFO ~100 Hz.
             * Poll every 5 ms.
             */
            osDelay(5);
        }
    }
}
