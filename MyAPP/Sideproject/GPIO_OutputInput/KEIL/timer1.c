#include "NuMicro.h"

void buzzer_tick_1ms(void);

void init_TIMER0(void)
{
    SYS_UnlockReg();

    /* Timer0 clock */
    CLK_EnableModuleClock(TMR0_MODULE);

    /* Timer0 clock source = PCLK0 */
    CLK_SetModuleClock(
        TMR0_MODULE,
        CLK_CLKSEL1_TMR0SEL_PCLK0,
        0
    );

    SYS_LockReg();

    TIMER_Open(
        TIMER0,
        TIMER_PERIODIC_MODE,
        1000
    );

    /* 開 Timer interrupt */
    TIMER_EnableInt(TIMER0);

    /* 開 NVIC */
    NVIC_EnableIRQ(TMR0_IRQn);

    /* Start */
    TIMER_Start(TIMER0);
}


void TMR0_IRQHandler(void)
{
    if(TIMER_GetIntFlag(TIMER0))
    {
        TIMER_ClearIntFlag(TIMER0);

        /* Notify the buzzer every 1 ms */
        buzzer_tick_1ms();
    }
}
