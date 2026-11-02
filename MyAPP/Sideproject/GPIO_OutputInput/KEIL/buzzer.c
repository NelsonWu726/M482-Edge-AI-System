
#include "NuMicro.h"

//-- buzzer.c
void init_buzzer(void);
void start_buzzer(void);
void stop_buzzer(void);

void buzzer_on(uint16_t ms);
//-----------


static volatile uint16_t bcnt = 0;


void init_EPWM1(uint16_t ch, uint16_t freq, uint16_t duty)
{
    SYS_UnlockReg();

    CLK_EnableModuleClock(EPWM1_MODULE);

    CLK_SetModuleClock(
        EPWM1_MODULE,
        CLK_CLKSEL2_EPWM1SEL_PCLK1,
        0
    );

    /* PC4 -> EPWM1_CH1 */
    SYS->GPC_MFPL =
        (SYS->GPC_MFPL & ~SYS_GPC_MFPL_PC4MFP_Msk)
        | SYS_GPC_MFPL_PC4MFP_EPWM1_CH1;

    SYS_LockReg();

    /* 4 kHz, 50% duty cycle */
    EPWM_ConfigOutputChannel(
        EPWM1,
        ch,
        freq,
        duty
    );
}


void init_buzzer(void)
{
    /* Configure EPWM: 4 kHz, 50% duty cycle */
    init_EPWM1(1, 4000, 50);

    /* Initially disable the PC4 PWM output */
    EPWM_DisableOutput(
        EPWM1,
        EPWM_CH_1_MASK
    );

    /* Keep the EPWM counter running continuously */
    EPWM_Start(
        EPWM1,
        EPWM_CH_1_MASK
    );
}


void start_buzzer(void)
{
    EPWM_EnableOutput(
        EPWM1,
        EPWM_CH_1_MASK
    );
}


void stop_buzzer(void)
{
    EPWM_DisableOutput(
        EPWM1,
        EPWM_CH_1_MASK
    );
}


/* Activate the buzzer for the specified duration in milliseconds */
void buzzer_on(uint16_t ms)
{
    bcnt = ms;
    start_buzzer();
}


/* Called by Timer0 every 1 ms */
void buzzer_tick_1ms(void)
{
    if(bcnt > 0)
    {
        bcnt--;

        if(bcnt == 0)
        {
            stop_buzzer();
        }
    }
}
