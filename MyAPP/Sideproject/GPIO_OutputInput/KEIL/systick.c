#include "NuMicro.h"
#include "cmsis_os.h"

/*
 * systick.c - RTX / CMSIS-RTOS safe compatibility version
 *
 * In this project, SysTick is owned by RTX.
 * Therefore this file must NOT define SysTick_Handler()
 * and must NOT call SysTick_Config().
 */

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6000000)
    #define WEAK __attribute__((weak))
#else
    #define WEAK __weak
#endif

/* Kept for compatibility with older code.
 * Do not use this variable as the RTOS system tick counter.
 */
volatile uint32_t msTicks = 0;


/* Optional weak callback retained for compatibility. */
WEAK void cb_systick(void)
{
}


/*
 * RTX already configures Cortex-M SysTick.
 * Do not configure it again here.
 */
void init_systick(void)
{
    /* Intentionally empty */
}


/*
 * Millisecond delay for the RTOS project.
 * Call this only after osKernelStart().
 */
void delay_ms(uint32_t ms)
{
    osDelay(ms);
}