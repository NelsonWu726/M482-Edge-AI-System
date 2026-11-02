#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "NuMicro.h"
#include "cmsis_os.h"


/* =========================================================
   WiFi Configuration
   ========================================================= */

#include "wifi_secrets.h"

#define WIFI_SERVER_PORT 8000


/* =========================================================
   Function Prototypes
   ========================================================= */

void wifi_init(void);

void esp8266_at(uint16_t n);

void wifi_recv(void);


int wifi_test_at(
    uint32_t timeout_ms
);

int wifi_test_connect(
    uint32_t timeout_ms
);

int wifi_get_ip(
    char *ip,
    uint32_t timeout_ms
);

int wifi_start_server(void);

int wifi_service_init(void);

int wifi_send(
    uint8_t client_id,
    const char *msg
);


void wifi_tick_thread(
    void const *argument
);


void wifi_debug_buffer(void);


static void esp_send(
    const char *s
);


static int wifi_wait_response(
    const char *target,
    uint32_t timeout_ms
);


static void wifi_update_link_state(void);


/* =========================================================
   lcd1602.c
   ========================================================= */

void lcd_print(
    uint8_t pos,
    char *p
);


/* =========================================================
   WiFi Thread
   ========================================================= */

osThreadDef(
    wifi_tick_thread,
    osPriorityNormal,
    1,
    0
);


osThreadId T_wifi_tick;


/* =========================================================
   WiFi RX Double Buffer
   ========================================================= */

static char rb1[2048];

static char rb2[2048];


static char *rb = rb1;

static char *rb0 = rb1;


/* =========================================================
   Receive State
   ========================================================= */

volatile char RCF = 0;


static volatile uint16_t rn = 0;

static volatile uint16_t ri = 0;

static volatile uint16_t tn = 0;


/* =========================================================
   WiFi Service State
   ========================================================= */

/*
 * 0 = initialization / unavailable
 * 1 = TCP server ready
 */

volatile uint8_t wifi_service_ready = 0;


/*
 * PC TCP client state
 */

volatile uint8_t wifi_client_ready = 0;

volatile uint8_t wifi_client_id = 0xFF;


/* =========================================================
   Time received from PC
   ========================================================= */

uint8_t wifi_hour = 0;

uint8_t wifi_min = 0;

uint8_t wifi_sec = 0;

uint8_t wifi_time_valid = 0;


/* =========================================================
   Debug
   ========================================================= */

volatile uint8_t wifi_cwjap_error = 0;

volatile uint32_t wifi_rx_count = 0;

volatile uint32_t wifi_connect_rx_start = 0;

volatile uint32_t wifi_connect_rx_end = 0;


/* =========================================================
   WiFi Initialization
   ========================================================= */

void wifi_init(void)
{
    rb = rb1;

    rb0 = rb1;


    RCF = 0;


    rn = 0;

    ri = 0;

    tn = 0;


    wifi_service_ready = 0;

    wifi_client_ready = 0;

    wifi_client_id = 0xFF;


    wifi_time_valid = 0;


    /* =====================================================
       Start WiFi timing thread

       Must be called after osKernelStart().
       ===================================================== */

    if(T_wifi_tick == NULL)
    {
        T_wifi_tick =
            osThreadCreate(
                osThread(wifi_tick_thread),
                NULL
            );
    }
}


/* =========================================================
   WiFi 1 ms Timing Thread

   UART packet is considered complete after
   approximately 10 ms without another byte.
   ========================================================= */

void wifi_tick_thread(
    void const *argument
)
{
    (void)argument;


    for(;;)
    {
        if(tn > 0)
        {
            tn--;


            if(tn == 0)
            {
                /* =========================================
                   Finish current RX buffer
                   ========================================= */

                rb[ri] = '\0';


                rn = ri;


                rb0 = rb;


                /* =========================================
                   Prepare next buffer
                   ========================================= */

                ri = 0;


                if(rb == rb1)
                {
                    rb = rb2;
                }
                else
                {
                    rb = rb1;
                }


                /* =========================================
                   Notify consumer
                   ========================================= */

                RCF = 1;
            }
        }


        osDelay(1);
    }
}


/* =========================================================
   Raw UART TX -> ESP8266
   ========================================================= */

static void esp_send(
    const char *s
)
{
    while(*s)
    {
        while(
            UART_IS_TX_FULL(
                UART1
            )
        )
        {
        }


        UART_WRITE(
            UART1,
            *s
        );


        s++;
    }


    while(
        !UART_GET_TX_EMPTY(
            UART1
        )
    )
    {
    }
}


/* =========================================================
   Raw Buffer Debug
   ========================================================= */

void wifi_debug_buffer(void)
{
    char show[17];

    uint8_t i;


    for(i = 0; i < 16; i++)
    {
        if(
            rb0[i] >= ' ' &&
            rb0[i] <= '~'
        )
        {
            show[i] =
                rb0[i];
        }
        else
        {
            show[i] =
                ' ';
        }
    }


    show[16] =
        '\0';


    lcd_print(
        0x00,
        "RX BUFFER       "
    );


    lcd_print(
        0x40,
        show
    );
}


/* =========================================================
   UART1 Interrupt
   ========================================================= */

void UART1_IRQHandler(void)
{
    char c;


    c =
        UART1->DAT;


    wifi_rx_count++;


    if(ri < 2047)
    {
        rb[ri++] =
            c;
    }


    /*
     * Restart 10 ms packet timer
     */
    tn = 10;
}


/* =========================================================
   AT Test
   ========================================================= */

int wifi_test_at(
    uint32_t timeout_ms
)
{
    RCF = 0;


    esp_send(
        "AT\r\n"
    );


    return
        wifi_wait_response(
            "OK",
            timeout_ms
        );
}


/* =========================================================
   Connect To Access Point
   ========================================================= */

int wifi_test_connect(
    uint32_t timeout_ms
)
{
    uint32_t t = 0;

    char *p;

    char command[160];


    wifi_cwjap_error = 0;


    /* =====================================================
       Station Mode
       ===================================================== */

    RCF = 0;


    esp_send(
        "AT+CWMODE=1\r\n"
    );


    if(
        !wifi_wait_response(
            "OK",
            2000
        )
    )
    {
        wifi_cwjap_error = 5;

        return 0;
    }


    /* =====================================================
       Connect AP
       ===================================================== */

    snprintf(
        command,
        sizeof(command),

        "AT+CWJAP=\"%s\",\"%s\"\r\n",

        WIFI_SSID,
        WIFI_PASSWORD
    );


    RCF = 0;


    wifi_connect_rx_start =
        wifi_rx_count;


    esp_send(
        command
    );


    while(t < timeout_ms)
    {
        if(RCF)
        {
            RCF = 0;


            /* =============================================
               Successful completion
               ============================================= */

            if(
                strstr(
                    rb0,
                    "OK"
                ) != NULL
            )
            {
                return 1;
            }


            /* =============================================
               ESP +CWJAP error
               ============================================= */

            p =
                strstr(
                    rb0,
                    "+CWJAP:"
                );


            if(p != NULL)
            {
                p += 7;


                if(
                    *p >= '0' &&
                    *p <= '9'
                )
                {
                    wifi_cwjap_error =
                        *p - '0';
                }


                return 0;
            }


            /* =============================================
               Generic Error
               ============================================= */

            if(
                strstr(
                    rb0,
                    "FAIL"
                ) != NULL
                ||
                strstr(
                    rb0,
                    "ERROR"
                ) != NULL
            )
            {
                wifi_cwjap_error = 6;

                return 0;
            }
        }


        osDelay(1);

        t++;
    }


    wifi_connect_rx_end =
        wifi_rx_count;


    wifi_cwjap_error = 7;


    return 0;
}


/* =========================================================
   Get ESP8266 IP
   ========================================================= */

int wifi_get_ip(
    char *ip,
    uint32_t timeout_ms
)
{
    uint32_t t = 0;

    char *p;

    uint8_t i;


    RCF = 0;


    esp_send(
        "AT+CIFSR\r\n"
    );


    while(t < timeout_ms)
    {
        if(RCF)
        {
            RCF = 0;


            p =
                strstr(
                    rb0,
                    "STAIP,\""
                );


            if(p != NULL)
            {
                p += 7;


                i = 0;


                while(
                    (*p != '"') &&
                    (*p != '\0') &&
                    (i < 15)
                )
                {
                    ip[i] =
                        *p;

                    i++;

                    p++;
                }


                ip[i] =
                    '\0';


                return 1;
            }
        }


        osDelay(1);

        t++;
    }


    return 0;
}


/* =========================================================
   Start TCP Server
   ========================================================= */

int wifi_start_server(void)
{
    char command[64];


    /* =====================================================
       Multiple Connections
       ===================================================== */

    RCF = 0;


    esp_send(
        "AT+CIPMUX=1\r\n"
    );


    if(
        !wifi_wait_response(
            "OK",
            2000
        )
    )
    {
        return 0;
    }


    /* =====================================================
       TCP Server
       ===================================================== */

    snprintf(
        command,
        sizeof(command),

        "AT+CIPSERVER=1,%u\r\n",

        WIFI_SERVER_PORT
    );


    RCF = 0;


    esp_send(
        command
    );


    if(
        !wifi_wait_response(
            "OK",
            2000
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Complete WiFi Service Initialization
   ========================================================= */

int wifi_service_init(void)
{
    int result;

    char ip[16];


    /* =====================================================
       Important:

       network_task MUST NOT handle RCF
       before this function finishes.
       ===================================================== */

    wifi_service_ready = 0;

    wifi_client_ready = 0;

    wifi_client_id = 0xFF;


    wifi_init();


    lcd_print(
        0x00,
        "ESP8266 TEST    "
    );


    lcd_print(
        0x40,
        "AT TEST...      "
    );


    osDelay(500);


    /* =====================================================
       1. AT Test
       ===================================================== */

    result =
        wifi_test_at(
            2000
        );


    if(!result)
    {
        lcd_print(
            0x00,
            "ESP8266 FAIL    "
        );


        lcd_print(
            0x40,
            "AT FAIL         "
        );


        return 0;
    }


    /* =====================================================
       2. Connect WiFi
       ===================================================== */

    lcd_print(
        0x00,
        "WIFI CONNECT    "
    );


    lcd_print(
        0x40,
        "CONNECTING...   "
    );


    result =
        wifi_test_connect(
            15000
        );


    if(!result)
    {
        lcd_print(
            0x00,
            "WIFI FAIL       "
        );


        if(wifi_cwjap_error == 1)
        {
            lcd_print(
                0x40,
                "ESP TIMEOUT     "
            );
        }
        else if(wifi_cwjap_error == 2)
        {
            lcd_print(
                0x40,
                "WRONG PASSWORD  "
            );
        }
        else if(wifi_cwjap_error == 3)
        {
            lcd_print(
                0x40,
                "AP NOT FOUND    "
            );
        }
        else if(wifi_cwjap_error == 4)
        {
            lcd_print(
                0x40,
                "CONNECT FAIL    "
            );
        }
        else if(wifi_cwjap_error == 5)
        {
            lcd_print(
                0x40,
                "CWMODE FAIL     "
            );
        }
        else if(wifi_cwjap_error == 6)
        {
            lcd_print(
                0x40,
                "ESP ERROR       "
            );
        }
        else
        {
            lcd_print(
                0x40,
                "CONNECT TIMEOUT "
            );
        }


        return 0;
    }


    osDelay(500);


    /* =====================================================
       3. Get IP
       ===================================================== */

    lcd_print(
        0x00,
        "WIFI CONNECTED  "
    );


    lcd_print(
        0x40,
        "GET IP...       "
    );


    result =
        wifi_get_ip(
            ip,
            3000
        );


    if(!result)
    {
        lcd_print(
            0x00,
            "WIFI FAIL       "
        );


        lcd_print(
            0x40,
            "GET IP FAIL     "
        );


        return 0;
    }


    /* =====================================================
       4. TCP Server
       ===================================================== */

    lcd_print(
        0x00,
        "START SERVER    "
    );


    lcd_print(
        0x40,
        "PORT 8000...    "
    );


    result =
        wifi_start_server();


    if(!result)
    {
        lcd_print(
            0x00,
            "SERVER FAIL     "
        );


        lcd_print(
            0x40,
            "PORT 8000 FAIL  "
        );


        return 0;
    }


    /* =====================================================
       WiFi Ready
       ===================================================== */

    wifi_service_ready = 1;


    lcd_print(
        0x00,
        "WIFI READY      "
    );


    lcd_print(
        0x40,
        ip
    );


    return 1;
}


/* =========================================================
   Parse TCP Link State
   ========================================================= */

static void wifi_update_link_state(void)
{
    char *p;

    uint8_t id;


    /* =====================================================
       Client Connected

       Example:
       0,CONNECT
       ===================================================== */

    p =
        strstr(
            rb0,
            ",CONNECT"
        );


    if(
        (p != NULL) &&
        (p > rb0)
    )
    {
        if(
            p[-1] >= '0' &&
            p[-1] <= '4'
        )
        {
            id =
                p[-1] - '0';


            wifi_client_id =
                id;


            wifi_client_ready =
                1;
        }
    }


    /* =====================================================
       Client Closed

       Example:
       0,CLOSED
       ===================================================== */

    p =
        strstr(
            rb0,
            ",CLOSED"
        );


    if(
        (p != NULL) &&
        (p > rb0)
    )
    {
        if(
            p[-1] >= '0' &&
            p[-1] <= '4'
        )
        {
            id =
                p[-1] - '0';


            if(
                wifi_client_id ==
                id
            )
            {
                wifi_client_ready =
                    0;


                wifi_client_id =
                    0xFF;
            }
        }
    }
}


/* =========================================================
   Receive TCP Data
   ========================================================= */

void wifi_recv(void)
{
    char *p;

    char *data;

    uint8_t client_id;


    /* =====================================================
       CONNECT / CLOSED
       ===================================================== */

    wifi_update_link_state();


    /* =====================================================
       Find +IPD

       Multiple connection format:

       +IPD,<id>,<len>:<data>
       ===================================================== */

    p =
        strstr(
            rb0,
            "+IPD,"
        );


    if(p == NULL)
    {
        return;
    }


    /* =====================================================
       Client ID
       ===================================================== */

    if(
        p[5] < '0' ||
        p[5] > '4'
    )
    {
        return;
    }


    client_id =
        p[5] - '0';


    /* =====================================================
       Remember PC connection
       ===================================================== */

    wifi_client_id =
        client_id;


    wifi_client_ready =
        1;


    /* =====================================================
       Find TCP Payload
       ===================================================== */

    data =
        strchr(
            p,
            ':'
        );


    if(data == NULL)
    {
        return;
    }


    data++;


    /* =====================================================
       PC Time Format

       HH:MM:SS
       ===================================================== */

    if(
        data[0] >= '0' &&
        data[0] <= '9' &&

        data[1] >= '0' &&
        data[1] <= '9' &&

        data[2] == ':' &&

        data[3] >= '0' &&
        data[3] <= '9' &&

        data[4] >= '0' &&
        data[4] <= '9' &&

        data[5] == ':' &&

        data[6] >= '0' &&
        data[6] <= '9' &&

        data[7] >= '0' &&
        data[7] <= '9'
    )
    {
        wifi_hour =
            (data[0] - '0') * 10 +
            (data[1] - '0');


        wifi_min =
            (data[3] - '0') * 10 +
            (data[4] - '0');


        wifi_sec =
            (data[6] - '0') * 10 +
            (data[7] - '0');


        /* =============================================
           Basic Range Check
           ============================================= */

        if(
            wifi_hour < 24 &&
            wifi_min < 60 &&
            wifi_sec < 60
        )
        {
            wifi_time_valid = 1;


            /*
             * ACK
             *
             * Add newline because TCP is a stream.
             */
            wifi_send(
                client_id,
                "RECEIVED\n"
            );
        }
    }
}


/* =========================================================
   Wait ESP8266 Response
   ========================================================= */

static int wifi_wait_response(
    const char *target,
    uint32_t timeout_ms
)
{
    uint32_t t = 0;


    while(t < timeout_ms)
    {
        if(RCF)
        {
            RCF = 0;


            /* =============================================
               Keep TCP state updated
               ============================================= */

            wifi_update_link_state();


            /* =============================================
               Expected Response
               ============================================= */

            if(
                strstr(
                    rb0,
                    target
                ) != NULL
            )
            {
                return 1;
            }


            /* =============================================
               Error
               ============================================= */

            if(
                strstr(
                    rb0,
                    "ERROR"
                ) != NULL
                ||
                strstr(
                    rb0,
                    "FAIL"
                ) != NULL
            )
            {
                return 0;
            }
        }


        osDelay(1);

        t++;
    }


    return 0;
}


/* =========================================================
   Send TCP Data
   ========================================================= */

int wifi_send(
    uint8_t client_id,
    const char *msg
)
{
    uint16_t len;

    char cmd[32];


    if(msg == NULL)
    {
        return 0;
    }


    if(client_id > 4)
    {
        return 0;
    }


    len =
        strlen(
            msg
        );


    if(len == 0)
    {
        return 0;
    }


    /* =====================================================
       AT+CIPSEND
       ===================================================== */

    snprintf(
        cmd,
        sizeof(cmd),

        "AT+CIPSEND=%u,%u\r\n",

        client_id,
        len
    );


    RCF = 0;


    esp_send(
        cmd
    );


    /* =====================================================
       Wait >
       ===================================================== */

    if(
        !wifi_wait_response(
            ">",
            1000
        )
    )
    {
        return 0;
    }


    /* =====================================================
       Send Payload
       ===================================================== */

    RCF = 0;


    esp_send(
        msg
    );


    /* =====================================================
       Wait SEND OK
       ===================================================== */

    if(
        !wifi_wait_response(
            "SEND OK",
            2000
        )
    )
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Manual AT Debug Function
   ========================================================= */

void esp8266_at(
    uint16_t n
)
{
    char cmd[160];


    switch(n)
    {
        case 0:

            esp_send(
                "AT+RST\r\n"
            );

            break;


        case 1:

            esp_send(
                "AT\r\n"
            );

            break;


        case 2:

            esp_send(
                "AT+CWMODE=1\r\n"
            );

            break;


        case 3:

            snprintf(
                cmd,
                sizeof(cmd),

                "AT+CWJAP=\"%s\",\"%s\"\r\n",

                WIFI_SSID,
                WIFI_PASSWORD
            );


            esp_send(
                cmd
            );

            break;


        case 4:

            esp_send(
                "AT+CIPMUX=1\r\n"
            );

            break;


        case 5:

            esp_send(
                "AT+CIFSR\r\n"
            );

            break;


        case 6:

            snprintf(
                cmd,
                sizeof(cmd),

                "AT+CIPSERVER=1,%u\r\n",

                WIFI_SERVER_PORT
            );


            esp_send(
                cmd
            );

            break;


        default:

            break;
    }
}