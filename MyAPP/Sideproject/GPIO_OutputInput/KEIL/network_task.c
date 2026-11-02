#include <stdio.h>
#include <stdint.h>

#include "cmsis_os.h"


/* =========================================================
   Device Configuration
   ========================================================= */

#define DEVICE_ID    "M482_01"


/* =========================================================
   Network Message Configuration

   IMPORTANT:
   Must stay IDENTICAL to:
       app_queue.c
       inference_task.c
       max30102_task.c
   ========================================================= */

#define NETWORK_MSG_ENV         1
#define NETWORK_MSG_PPG         2
#define NETWORK_MSG_HEART       3

#define PPG_BATCH_SIZE         10


/*
 * MAX30102 raw FIFO is about 100 Hz.
 * max30102_task.c keeps 1 out of every 5 samples.
 */
#define PPG_NETWORK_SAMPLE_RATE_HZ    20


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
   wifi.c
   ========================================================= */

void wifi_recv(void);

int wifi_send(
    uint8_t client_id,
    const char *msg
);

extern volatile char RCF;

extern volatile uint8_t wifi_service_ready;

extern volatile uint8_t wifi_client_ready;
extern volatile uint8_t wifi_client_id;

extern volatile uint32_t wifi_rx_count;


/* =========================================================
   TX Buffer

   Static/global:
   do not place this large buffer on RTOS task stack.
   ========================================================= */

static char g_network_tx_buffer[640];


/* =========================================================
   Debug Counters
   ========================================================= */

volatile uint32_t network_loop_count = 0;

volatile uint32_t network_rcf_count = 0;
volatile uint32_t network_last_wifi_rx = 0;
volatile uint32_t network_rx_count = 0;

volatile uint32_t network_tx_ok = 0;
volatile uint32_t network_tx_fail = 0;

volatile uint32_t network_drop_no_client = 0;

volatile uint32_t network_queue_rx_count = 0;

volatile uint32_t network_env_rx_count = 0;
volatile uint32_t network_ppg_rx_count = 0;
volatile uint32_t network_heart_rx_count = 0;

volatile uint32_t network_json_build_fail = 0;



/* =========================================================
   Small JSON Append Helpers
   ========================================================= */

static uint8_t network_append_char(
    char *buffer,
    uint32_t buffer_size,
    uint32_t *pos,
    char c
)
{
    if(
        buffer == 0 ||
        pos == 0 ||
        *pos >= (buffer_size - 1)
    )
    {
        return 0;
    }


    buffer[*pos] = c;

    (*pos)++;

    buffer[*pos] = '\0';


    return 1;
}


static uint8_t network_append_string(
    char *buffer,
    uint32_t buffer_size,
    uint32_t *pos,
    const char *text
)
{
    if(
        buffer == 0 ||
        pos == 0 ||
        text == 0
    )
    {
        return 0;
    }


    while(*text)
    {
        if(
            !network_append_char(
                buffer,
                buffer_size,
                pos,
                *text
            )
        )
        {
            return 0;
        }


        text++;
    }


    return 1;
}


static uint8_t network_append_u32(
    char *buffer,
    uint32_t buffer_size,
    uint32_t *pos,
    uint32_t value
)
{
    char digits[10];

    uint8_t count = 0;


    if(value == 0)
    {
        return
            network_append_char(
                buffer,
                buffer_size,
                pos,
                '0'
            );
    }


    while(
        value > 0 &&
        count < sizeof(digits)
    )
    {
        digits[count] =
            (char)(
                '0' +
                (value % 10)
            );

        count++;

        value /= 10;
    }


    while(count > 0)
    {
        count--;


        if(
            !network_append_char(
                buffer,
                buffer_size,
                pos,
                digits[count]
            )
        )
        {
            return 0;
        }
    }


    return 1;
}


/* =========================================================
   Build ENV JSON

   Example:
   {
      "type":"env",
      "device":"M482_01",
      "seq":0,
      "temp_x10":275,
      "lux":350,
      "ai":1
   }
   ========================================================= */

static uint8_t network_build_env_json(
    char *buffer,
    uint32_t buffer_size,
    uint32_t sequence,
    const network_message_t *msg
)
{
    int written;

    int temp10;


    temp10 =
        (int)(
            msg->temperature *
            10.0f
        );


    written =
        snprintf(
            buffer,
            buffer_size,

            "{\"type\":\"env\","
            "\"device\":\"%s\","
            "\"seq\":%lu,"
            "\"temp_x10\":%d,"
            "\"lux\":%u,"
            "\"ai\":%u}\n",

            DEVICE_ID,

            (unsigned long)
            sequence,

            temp10,

            msg->lux,

            msg->ai_result
        );


    if(written < 0)
    {
        return 0;
    }


    if((uint32_t)written >= buffer_size)
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Build HEART JSON

   Example:
   {
      "type":"heart",
      "device":"M482_01",
      "seq":10,
      "bpm":81,
      "bpm_valid":1,
      "spo2":98,
      "spo2_valid":1,
      "finger":1
   }
   ========================================================= */

static uint8_t network_build_heart_json(
    char *buffer,
    uint32_t buffer_size,
    uint32_t sequence,
    const network_message_t *msg
)
{
    int written;


    written =
        snprintf(
            buffer,
            buffer_size,

            "{\"type\":\"heart\","
            "\"device\":\"%s\","
            "\"seq\":%lu,"
            "\"bpm\":%u,"
            "\"bpm_valid\":%u,"
            "\"spo2\":%u,"
            "\"spo2_valid\":%u,"
            "\"finger\":%u}\n",

            DEVICE_ID,

            (unsigned long)
            sequence,

            (unsigned int)
            msg->bpm,

            (unsigned int)
            msg->bpm_valid,

            (unsigned int)
            msg->spo2,

            (unsigned int)
            msg->spo2_valid,

            (unsigned int)
            msg->finger_present
        );


    if(written < 0)
    {
        return 0;
    }


    if((uint32_t)written >= buffer_size)
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Build PPG JSON

   Example:
   {
      "type":"ppg",
      "device":"M482_01",
      "seq":1,
      "fs":20,
      "count":10,
      "red":[...],
      "ir":[...]
   }
   ========================================================= */

static uint8_t network_build_ppg_json(
    char *buffer,
    uint32_t buffer_size,
    uint32_t sequence,
    const network_message_t *msg
)
{
    uint32_t pos = 0;

    uint8_t i;

    uint8_t count;


    if(
        buffer == 0 ||
        msg == 0 ||
        buffer_size < 2
    )
    {
        return 0;
    }


    buffer[0] = '\0';


    count =
        msg->ppg_count;


    if(count > PPG_BATCH_SIZE)
    {
        count =
            PPG_BATCH_SIZE;
    }


    /* Header */

    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            "{\"type\":\"ppg\",\"device\":\""
        )
    )
    {
        return 0;
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            DEVICE_ID
        )
    )
    {
        return 0;
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            "\",\"seq\":"
        )
    )
    {
        return 0;
    }


    if(
        !network_append_u32(
            buffer,
            buffer_size,
            &pos,
            sequence
        )
    )
    {
        return 0;
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            ",\"fs\":"
        )
    )
    {
        return 0;
    }


    if(
        !network_append_u32(
            buffer,
            buffer_size,
            &pos,
            PPG_NETWORK_SAMPLE_RATE_HZ
        )
    )
    {
        return 0;
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            ",\"count\":"
        )
    )
    {
        return 0;
    }


    if(
        !network_append_u32(
            buffer,
            buffer_size,
            &pos,
            count
        )
    )
    {
        return 0;
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            ",\"red\":["
        )
    )
    {
        return 0;
    }


    /* RED array */

    for(i = 0; i < count; i++)
    {
        if(i != 0)
        {
            if(
                !network_append_char(
                    buffer,
                    buffer_size,
                    &pos,
                    ','
                )
            )
            {
                return 0;
            }
        }


        if(
            !network_append_u32(
                buffer,
                buffer_size,
                &pos,
                msg->red[i]
            )
        )
        {
            return 0;
        }
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            "],\"ir\":["
        )
    )
    {
        return 0;
    }


    /* IR array */

    for(i = 0; i < count; i++)
    {
        if(i != 0)
        {
            if(
                !network_append_char(
                    buffer,
                    buffer_size,
                    &pos,
                    ','
                )
            )
            {
                return 0;
            }
        }


        if(
            !network_append_u32(
                buffer,
                buffer_size,
                &pos,
                msg->ir[i]
            )
        )
        {
            return 0;
        }
    }


    if(
        !network_append_string(
            buffer,
            buffer_size,
            &pos,
            "]}\n"
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Network Task
   ========================================================= */

void network_task(
    void const *argument
)
{
    osEvent event;

    network_message_t *result;

    uint32_t sequence = 0;

    uint8_t json_ok;


    (void)argument;


    for(;;)
    {
        /* Task Alive Counter */

        network_loop_count++;


        /* UART Debug */

        network_last_wifi_rx =
            wifi_rx_count;


        /* Wait until WiFi service is ready */

        if(!wifi_service_ready)
        {
            osDelay(10);

            continue;
        }


        /* ESP8266 -> MCU */

        if(RCF)
        {
            RCF = 0;

            network_rcf_count++;

            network_rx_count++;

            wifi_recv();
        }


        /* Get one outgoing message */

        event =
            osMailGet(
                network_queue_id,
                0
            );


        if(event.status == osEventMail)
        {
            network_queue_rx_count++;


            result =
                (network_message_t *)
                event.value.p;


            json_ok = 0;


            /* ENV */

            if(
                result->type ==
                NETWORK_MSG_ENV
            )
            {
                network_env_rx_count++;


                json_ok =
                    network_build_env_json(
                        g_network_tx_buffer,
                        sizeof(g_network_tx_buffer),
                        sequence,
                        result
                    );
            }


            /* PPG */

            else if(
                result->type ==
                NETWORK_MSG_PPG
            )
            {
                network_ppg_rx_count++;


                json_ok =
                    network_build_ppg_json(
                        g_network_tx_buffer,
                        sizeof(g_network_tx_buffer),
                        sequence,
                        result
                    );
            }


            /* HEART */

            else if(
                result->type ==
                NETWORK_MSG_HEART
            )
            {
                network_heart_rx_count++;


                json_ok =
                    network_build_heart_json(
                        g_network_tx_buffer,
                        sizeof(g_network_tx_buffer),
                        sequence,
                        result
                    );
            }


            /* Send To PC */

            if(json_ok)
            {
                if(wifi_client_ready)
                {
                    if(
                        wifi_send(
                            wifi_client_id,
                            g_network_tx_buffer
                        )
                    )
                    {
                        network_tx_ok++;

                        sequence++;
                    }
                    else
                    {
                        network_tx_fail++;
                    }
                }
                else
                {
                    network_drop_no_client++;
                }
            }
            else
            {
                network_json_build_fail++;
            }


            /* Release Mail */

            osMailFree(
                network_queue_id,
                result
            );
        }


        /*
         * Keep network thread cooperative with:
         * sensor / inference / output / MAX30102.
         */
        osDelay(10);
    }
}
