#include <stdint.h>

#include "NuMicro.h"
#include "cmsis_os.h"


/* =========================================================
   External APIs
   ========================================================= */

/* hclk.c */
void init_HCLK_HXT_192M(void);


/* uart.c */
void init_UART1(void);

/* usb_hid_port.c */
void usb_hid_init(void);

/* lcd1602.c */
void lcd_init(void);

void lcd_print(
    uint8_t pos,
    char *p
);


/* test.c */
uint8_t boot_self_test(void);


/* wifi.c */
int wifi_service_init(void);


/* app_queue.c */
int app_queue_init(void);


/* myi2c.c */
int i2c_mutex_init(void);


/* ai_model.cpp */
int ai_model_test(void);


/* sensor_task.c */
void sensor_task(
    void const *argument
);


/* inference_task.c */
void inference_task(
    void const *argument
);


/* output_task.c */
void output_task(
    void const *argument
);


/* network_task.c */
void network_task(
    void const *argument
);


/* max30102_task.c */
void max30102_task(
    void const *argument
);

/* usb_hid_task.c */
void usb_hid_task(void const *argument);
/* =========================================================
   Application Initialization Task
   ========================================================= */

static void app_init_task(
    void const *argument
);


/* =========================================================
   Debug / Runtime State

   These variables are intentionally global so they can
   be inspected directly in Keil Watch.
   ========================================================= */

/*
 * Initialization stages:
 *
 * 0  = not started
 * 1  = app_init_task started
 * 2  = LCD initialized
 * 3  = AI model test passed
 * 4  = boot self-test completed
 * 5  = WiFi service ready
 * 6  = queues ready
 * 7  = sensor task created
 * 8  = inference task created
 * 9  = MAX30102 task created
 * 10 = output task created
 * 11 = network task created
 * 12 = initialization complete
 */

volatile uint32_t app_init_stage = 0;


/*
 * Queue init return value:
 *
 *  0 = success
 * -1 = sensor queue failed
 * -2 = result queue failed
 * -3 = network queue failed
 */

volatile int32_t app_queue_status_debug = -99;


/*
 * WiFi init return value:
 *
 * 0 = failed
 * 1 = success
 */

volatile int32_t app_wifi_status_debug = -99;


/*
 * AI test return value
 */

volatile int32_t app_ai_status_debug = -99;


/*
 * I2C mutex init return value:
 *
 * 0 = failed
 * 1 = success
 */
volatile int32_t app_i2c_mutex_status_debug = -99;


/*
 * Thread creation failure code:
 *
 * 0 = none
 * 1 = app_init_task
 * 2 = sensor_task
 * 3 = inference_task
 * 4 = output_task
 * 5 = network_task
 * 6 = max30102_task
 */

volatile uint8_t app_thread_error = 0;


/* =========================================================
   Thread IDs

   Inspect these in Keil Watch.
   NULL means thread creation failed / not reached.
   ========================================================= */

osThreadId app_init_task_id = NULL;

osThreadId sensor_task_id = NULL;

osThreadId inference_task_id = NULL;

osThreadId output_task_id = NULL;

osThreadId network_task_id = NULL;

osThreadId max30102_task_id = NULL;

osThreadId usb_hid_task_id = NULL;

/* =========================================================
   RTOS Task Definitions
   ========================================================= */

/*
 * Initialization task needs relatively large stack because
 * it performs TFLite setup / tests and WiFi initialization.
 */
osThreadDef(
    app_init_task,
    osPriorityNormal,
    1,
    4096
);


/*
 * Sensor task
 */
osThreadDef(
    sensor_task,
    osPriorityNormal,
    1,
    0
);


/*
 * TFLite Micro inference task
 */
osThreadDef(
    inference_task,
    osPriorityNormal,
    1,
    4096
);


/*
 * MAX30102 task
 */
osThreadDef(
    max30102_task,
    osPriorityNormal,
    1,
    0
);


/*
 * LCD / buzzer output task
 */
osThreadDef(
    output_task,
    osPriorityNormal,
    1,
    0
);


/*
 * WiFi / TCP network task
 */
osThreadDef(
    network_task,
    osPriorityNormal,
    1,
    0
);

/* usb_hid_task.c */


/*
 * USB HID Button Task
 */
 

osThreadDef(
			usb_hid_task,
			osPriorityNormal,
			1,
			512
);

/* =========================================================
   Application Start
   ========================================================= */

void app_start(void)
{
    int32_t kernel_status;


    /* =====================================================
       System Clock
       ===================================================== */

    init_HCLK_HXT_192M();


    /* =====================================================
       UART1

       UART1 is used by ESP8266.
       ===================================================== */

    init_UART1();

		
		/* ======================================
				USB HID Initialization
			 ====================================== */

		usb_hid_init();

    /* =====================================================
       RTOS Kernel Initialize
       ===================================================== */

    kernel_status =
        osKernelInitialize();


    /*
     * CMSIS-RTOS initialization failure.
     *
     * LCD is not initialized yet, so only debug state
     * can be used here.
     */
    if(kernel_status != osOK)
    {
        app_thread_error = 1;

        for(;;)
        {
        }
    }


    /* =====================================================
       Create Initialization Task
       ===================================================== */

    app_init_task_id =
        osThreadCreate(
            osThread(app_init_task),
            NULL
        );


    if(app_init_task_id == NULL)
    {
        app_thread_error = 1;

        for(;;)
        {
        }
    }


    /* =====================================================
       Start RTOS Scheduler
       ===================================================== */

    kernel_status =
        osKernelStart();


    /*
     * Normally osKernelStart() does not return after
     * successful scheduler startup.
     *
     * If execution reaches here with an error,
     * preserve debug information.
     */
    if(kernel_status != osOK)
    {
        app_thread_error = 1;

        for(;;)
        {
        }
    }
}


/* =========================================================
   Application Initialization Task
   ========================================================= */

static void app_init_task(
    void const *argument
)
{
    int queue_status;

    int ai_status;

    int wifi_status;


    (void)argument;


    /* =====================================================
       Initialization Started
       ===================================================== */

    app_init_stage = 1;


    /* =====================================================
       Wait System Stable
       ===================================================== */

    osDelay(1000);


    /* =====================================================
       LCD Initialization
       ===================================================== */

    lcd_init();


    app_init_stage = 2;


    /* =====================================================
       Shared I2C Bus Mutex

       BH1750 and MAX30102 both use:
           SDA = PB4
           SCL = PB5

       Create one mutex before either sensor task starts.
       ===================================================== */

    app_i2c_mutex_status_debug =
        i2c_mutex_init();


    if(!app_i2c_mutex_status_debug)
    {
        lcd_print(
            0x00,
            "I2C MUTEX ERROR "
        );


        lcd_print(
            0x40,
            "INIT FAILED     "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    /* =====================================================
       TFLite Micro Operator Test
       ===================================================== */

    lcd_print(
        0x00,
        "TFLM TEST       "
    );


    lcd_print(
        0x40,
        "ALL 6 OPS       "
    );


    osDelay(500);


    ai_status =
        ai_model_test();


    app_ai_status_debug =
        ai_status;


    /* =====================================================
       TFLite Error
       ===================================================== */

    if(ai_status != 0)
    {
        lcd_print(
            0x00,
            "TFLM ERROR      "
        );


        if(ai_status == -1)
        {
            lcd_print(
                0x40,
                "EXPAND DIMS     "
            );
        }
        else if(ai_status == -2)
        {
            lcd_print(
                0x40,
                "CONV2D          "
            );
        }
        else if(ai_status == -3)
        {
            lcd_print(
                0x40,
                "RESHAPE         "
            );
        }
        else if(ai_status == -4)
        {
            lcd_print(
                0x40,
                "MEAN            "
            );
        }
        else if(ai_status == -5)
        {
            lcd_print(
                0x40,
                "FULL CONNECT    "
            );
        }
        else if(ai_status == -6)
        {
            lcd_print(
                0x40,
                "LOGISTIC        "
            );
        }
        else if(ai_status == -7)
        {
            lcd_print(
                0x40,
                "MODEL DATA      "
            );
        }
        else
        {
            lcd_print(
                0x40,
                "UNKNOWN         "
            );
        }


        for(;;)
        {
            osDelay(1000);
        }
    }


    /* =====================================================
       TFLite Test Passed
       ===================================================== */

    app_init_stage = 3;


    lcd_print(
        0x00,
        "TFLM OP OK      "
    );


    lcd_print(
        0x40,
        "ALL 6 OPS OK    "
    );


    osDelay(1000);


    /* =====================================================
       Boot Self Test
       ===================================================== */

    lcd_print(
        0x00,
        "BOOT SELF TEST  "
    );


    lcd_print(
        0x40,
        "START...        "
    );


    osDelay(500);


    boot_self_test();


    app_init_stage = 4;


    /* =====================================================
       Network Initialization
       ===================================================== */

    lcd_print(
        0x00,
        "NETWORK INIT    "
    );


    lcd_print(
        0x40,
        "START...        "
    );


    osDelay(500);


    wifi_status =
        wifi_service_init();


    app_wifi_status_debug =
        wifi_status;


    if(!wifi_status)
    {
        lcd_print(
            0x00,
            "NETWORK ERROR   "
        );


        lcd_print(
            0x40,
            "WIFI INIT FAIL  "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 5;


    /* =====================================================
       Create Application Queues
       ===================================================== */

    queue_status =
        app_queue_init();


    app_queue_status_debug =
        queue_status;


    if(queue_status != 0)
    {
        lcd_print(
            0x00,
            "QUEUE ERROR     "
        );


        if(queue_status == -1)
        {
            lcd_print(
                0x40,
                "SENSOR QUEUE    "
            );
        }
        else if(queue_status == -2)
        {
            lcd_print(
                0x40,
                "RESULT QUEUE    "
            );
        }
        else if(queue_status == -3)
        {
            lcd_print(
                0x40,
                "NETWORK QUEUE   "
            );
        }
        else
        {
            lcd_print(
                0x40,
                "UNKNOWN QUEUE   "
            );
        }


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 6;
		
		/* =====================================================
			 Create USB HID Button Task
			 ===================================================== */

		usb_hid_task_id =
				osThreadCreate(
						osThread(usb_hid_task),
						NULL
				);

		if (usb_hid_task_id == NULL)
		{
				app_thread_error = 7;

				for (;;)
				{
						osDelay(1000);
				}
		}


    /* =====================================================
       Create Sensor Task
       ===================================================== */

    sensor_task_id =
        osThreadCreate(
            osThread(sensor_task),
            NULL
        );


    if(sensor_task_id == NULL)
    {
        app_thread_error = 2;


        lcd_print(
            0x00,
            "THREAD ERROR    "
        );


        lcd_print(
            0x40,
            "SENSOR TASK     "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 7;


    /* =====================================================
       Create Inference Task
       ===================================================== */

    inference_task_id =
        osThreadCreate(
            osThread(inference_task),
            NULL
        );


    if(inference_task_id == NULL)
    {
        app_thread_error = 3;


        lcd_print(
            0x00,
            "THREAD ERROR    "
        );


        lcd_print(
            0x40,
            "INFER TASK      "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 8;


    /* =====================================================
       Create MAX30102 Task
       ===================================================== */

    max30102_task_id =
        osThreadCreate(
            osThread(max30102_task),
            NULL
        );


    if(max30102_task_id == NULL)
    {
        app_thread_error = 6;


        lcd_print(
            0x00,
            "THREAD ERROR    "
        );


        lcd_print(
            0x40,
            "MAX30102 TASK   "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 9;


    /* =====================================================
       Create Output Task
       ===================================================== */

    output_task_id =
        osThreadCreate(
            osThread(output_task),
            NULL
        );


    if(output_task_id == NULL)
    {
        app_thread_error = 4;


        lcd_print(
            0x00,
            "THREAD ERROR    "
        );


        lcd_print(
            0x40,
            "OUTPUT TASK     "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 10;


    /* =====================================================
       Create Network Task
       ===================================================== */

    network_task_id =
        osThreadCreate(
            osThread(network_task),
            NULL
        );


    if(network_task_id == NULL)
    {
        app_thread_error = 5;


        lcd_print(
            0x00,
            "THREAD ERROR    "
        );


        lcd_print(
            0x40,
            "NETWORK TASK    "
        );


        for(;;)
        {
            osDelay(1000);
        }
    }


    app_init_stage = 11;


    /* =====================================================
       Initialization Complete
       ===================================================== */

    lcd_print(
        0x00,
        "SYSTEM READY    "
    );


    lcd_print(
        0x40,
        "RTOS RUNNING    "
    );


    app_init_stage = 12;


    /* =====================================================
       Initialization Task No Longer Needed
       ===================================================== */

    osThreadTerminate(
        osThreadGetId()
    );
		

    /*
     * Should never reach here.
     */
    for(;;)
    {
        osDelay(1000);
    }
}