/*----------------------------------------------------------------------------
 *      CMSIS-RTOS  -  RTX
 *----------------------------------------------------------------------------
 *      Name:    RTX_Conf_CM.C
 *      Purpose: Configuration of CMSIS RTX Kernel for Cortex-M
 *      Rev.:    V4.70.1 + project AC6/RTX4.82 include fix
 *----------------------------------------------------------------------------*/

#include "cmsis_os.h"

/*
 * ============================================================
 * Project compatibility note
 * ============================================================
 *
 * This project uses:
 *
 *     Arm Compiler 6
 *     CMSIS-RTOS v1
 *     RTX 4.82
 *
 * Absolute include paths are intentionally used so an older
 * RTX_CM_lib.h from another CMSIS installation is not selected.
 *
 * After the project is stable, these may be changed back to
 * normal relative includes if the Keil include paths are
 * confirmed to use ARM::CMSIS 5.6.0 only.
 */


/*----------------------------------------------------------------------------
 *      RTX User configuration part BEGIN
 *---------------------------------------------------------------------------*/


//-------- <<< Use Configuration Wizard in Context Menu >>> -----------------
//
// <h>Thread Configuration
// =======================
//
//   <o>Number of concurrent running user threads <1-250>
//   <i> Defines max. number of user threads that will run at the same time.
//
// Current project:
//
//   main thread
//
//   app_init_task
//   wifi_tick_thread
//   sensor_task
//   inference_task
//   output_task
//   network_task
//
// During initialization, app_init_task still exists while the other
// application tasks are being created.
//
// Keep additional capacity for debugging / future tasks.
//
// Recommended: 8
//

#ifndef OS_TASKCNT
#define OS_TASKCNT        10
#endif


//   <o>Default Thread stack size [bytes] <64-4096:8><#/4>
//   <i> Defines default stack size for threads with osThreadDef stacksz = 0
//
// 256 words * 4 bytes = 1024 bytes
//
// Used by:
//
//   wifi_tick_thread
//   sensor_task
//   output_task
//   network_task
//

#ifndef OS_STKSIZE
#define OS_STKSIZE        256
#endif


//   <o>Main Thread stack size [bytes] <64-32768:8><#/4>
//   <i> Defines stack size for main thread.
//
// 50 words * 4 bytes = 200 bytes
//
// main() only calls app_start(), therefore this is currently sufficient.
//

#ifndef OS_MAINSTKSIZE
#define OS_MAINSTKSIZE    50
#endif


//   <o>Number of threads with user-provided stack size <0-250>
//   <i> Defines the number of threads with user-provided stack size.
//
// Two tasks currently use explicit 4096-byte stacks:
//
//   app_init_task
//   inference_task
//

#ifndef OS_PRIVCNT
#define OS_PRIVCNT        3
#endif


//   <o>Total stack size [bytes] for threads with user-provided stack size
//       <0-1048576:8><#/4>
//
// app_init_task:
//     4096 bytes
//
// inference_task:
//     4096 bytes
//
// Total:
//     8192 bytes
//
// This configuration value is in WORDS:
//
//     8192 / 4 = 2048 words
//

#ifndef OS_PRIVSTKSIZE
#define OS_PRIVSTKSIZE    3072
#endif


//   <q>Stack overflow checking
//   <i> Enable stack overflow checks at thread switch.
//
// Keep enabled.
//
// If a stack overflow occurs, os_error() below records it in
// rtx_error_code and rtx_error_thread.
//

#ifndef OS_STKCHECK
#define OS_STKCHECK       1
#endif


//   <q>Stack usage watermark
//   <i> Initialize thread stack with watermark pattern for analyzing stack
//       usage.
//
// Keep disabled during normal execution because stack initialization makes
// osThreadCreate() slower.
//
// Temporarily set to 1 when investigating stack usage.
//

#ifndef OS_STKINIT
#define OS_STKINIT        0
#endif


//   <o>Processor mode for thread execution
//     <0=> Unprivileged mode
//     <1=> Privileged mode
//
// Keep privileged mode for this embedded application.
//

#ifndef OS_RUNPRIV
#define OS_RUNPRIV        1
#endif


// </h>


// <h>RTX Kernel Timer Tick Configuration
// ======================================
//
//   <q> Use Cortex-M SysTick timer as RTX Kernel Timer
//
// RTX owns SysTick.
//
// Application code should therefore use:
//
//     osDelay()
//
// instead of implementing another SysTick timebase.
//

#ifndef OS_SYSTICK
#define OS_SYSTICK        1
#endif


//   <o>RTOS Kernel Timer input clock frequency [Hz]
//
// M482 core clock:
//
//     192 MHz
//

#ifndef OS_CLOCK
#define OS_CLOCK          192000000
#endif


//   <o>RTX Timer tick interval value [us] <1-1000000>
//
// 1000 us = 1 ms
//
// Therefore:
//
//     osDelay(1)    = approximately 1 ms
//     osDelay(1000) = approximately 1 second
//

#ifndef OS_TICK
#define OS_TICK           1000
#endif


// </h>


// <h>System Configuration
// =======================
//
// <e>Round-Robin Thread switching
// ===============================
//
// Required because most application tasks currently use
// osPriorityNormal.
//

#ifndef OS_ROBIN
#define OS_ROBIN          1
#endif


//   <o>Round-Robin Timeout [ticks] <1-1000>
//
// 5 RTX ticks:
//
//     5 * 1 ms = 5 ms
//

#ifndef OS_ROBINTOUT
#define OS_ROBINTOUT      5
#endif


// </e>


// <e>User Timers
// ==============
//
// Keep enabled for CMSIS-RTOS timer compatibility.
//

#ifndef OS_TIMERS
#define OS_TIMERS         1
#endif


//   <o>Timer Thread Priority
//                        <1=> Low
//     <2=> Below Normal  <3=> Normal  <4=> Above Normal
//                        <5=> High
//                        <6=> Realtime (highest)

#ifndef OS_TIMERPRIO
#define OS_TIMERPRIO      5
#endif


//   <o>Timer Thread stack size [bytes] <64-4096:8><#/4>
//
// 50 words * 4 = 200 bytes
//

#ifndef OS_TIMERSTKSZ
#define OS_TIMERSTKSZ     50
#endif


//   <o>Timer Callback Queue size <1-32>

#ifndef OS_TIMERCBQS
#define OS_TIMERCBQS      4
#endif


// </e>


//   <o>ISR FIFO Queue size
//
// UART1_IRQHandler() currently handles the ESP8266 UART bytes directly
// and does not post CMSIS messages from the ISR, therefore 16 is
// sufficient for the current design.
//

#ifndef OS_FIFOSZ
#define OS_FIFOSZ         16
#endif


// </h>


//------------- <<< end of configuration section >>> -----------------------


/* =========================================================
   Standard Library System Mutexes
   ========================================================= */

#ifndef OS_MUTEXCNT
#define OS_MUTEXCNT       8
#endif


/*----------------------------------------------------------------------------
 *      RTX User configuration part END
 *---------------------------------------------------------------------------*/


#define OS_TRV \
    ((uint32_t)(((double)OS_CLOCK * (double)OS_TICK) / 1E6) - 1)


/* =========================================================
   RTX Debug State

   Add these variables to Keil Watch when debugging RTX.

   rtx_error_code:

       0 = no RTX runtime error

       1 = stack overflow
       2 = ISR FIFO overflow
       3 = mailbox overflow
       4 = timer callback queue overflow

   rtx_error_thread:

       offending thread when available
   ========================================================= */

volatile uint32_t rtx_error_code = 0;

volatile osThreadId rtx_error_thread = NULL;


/*----------------------------------------------------------------------------
 *      Global Functions
 *---------------------------------------------------------------------------*/


/*--------------------------- os_idle_demon --------------------------------*/

/**
 * Idle thread.
 *
 * Runs whenever no application thread is ready.
 */

void os_idle_demon(void)
{
    for(;;)
    {
        /*
         * Optional:
         *
         * Low-power mode could eventually be added here.
         *
         * Example:
         *
         * __WFI();
         *
         * Do not enable it until all interrupt / wake-up
         * behavior has been verified.
         */
    }
}


/* =========================================================
   Alternative RTX Timer

   Not used because:

       OS_SYSTICK = 1
   ========================================================= */

#if (OS_SYSTICK == 0)


/*--------------------------- os_tick_init ---------------------------------*/

int os_tick_init(void)
{
    return -1;
}


/*--------------------------- os_tick_val ----------------------------------*/

uint32_t os_tick_val(void)
{
    return 0;
}


/*--------------------------- os_tick_ovf ----------------------------------*/

uint32_t os_tick_ovf(void)
{
    return 0;
}


/*--------------------------- os_tick_irqack -------------------------------*/

void os_tick_irqack(void)
{
}


#endif


/* =========================================================
   RTX Runtime Error Handling
   ========================================================= */


/* RTX Error Codes */

#define OS_ERROR_STACK_OVF      1

#define OS_ERROR_FIFO_OVF       2

#define OS_ERROR_MBX_OVF        3

#define OS_ERROR_TIMER_OVF      4


extern osThreadId svcThreadGetId(void);


/**
 * Called by RTX when a fatal runtime error is detected.
 *
 * Debug with Keil Watch:
 *
 *     rtx_error_code
 *     rtx_error_thread
 */

void os_error(
    uint32_t error_code
)
{
    /* =====================================================
       Save Error Code
       ===================================================== */

    rtx_error_code =
        error_code;


    /* =====================================================
       Save Current Thread
       ===================================================== */

    rtx_error_thread =
        svcThreadGetId();


    /* =====================================================
       Error Classification
       ===================================================== */

    switch(error_code)
    {
        case OS_ERROR_STACK_OVF:

            /*
             * Stack overflow.
             *
             * Inspect:
             *
             *     rtx_error_thread
             *
             * to identify the offending task.
             */

            break;


        case OS_ERROR_FIFO_OVF:

            /*
             * RTX ISR FIFO overflow.
             */

            break;


        case OS_ERROR_MBX_OVF:

            /*
             * RTX mailbox / mail queue overflow.
             */

            break;


        case OS_ERROR_TIMER_OVF:

            /*
             * RTX timer callback queue overflow.
             */

            break;


        default:

            /*
             * Unknown RTX runtime error.
             */

            break;
    }


    /*
     * Fatal RTX error:
     *
     * stop here so the state remains visible
     * in the debugger.
     */

    for(;;)
    {
    }
}


/* =========================================================
   RTX Kernel Implementation
   ========================================================= */

#include "RTX_CM_lib.h"

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/