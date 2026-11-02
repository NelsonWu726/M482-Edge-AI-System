#include "NuMicro.h"

void init_TIMER0(void)
{
    SYS_UnlockReg();
	
  
    CLK_EnableModuleClock(TMR0_MODULE);

  
    CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_PCLK0, 0);


    SYS->GPB_MFPL |= SYS_GPB_MFPL_PB5MFP_TM0;

 
    SYS_LockReg();

    TIMER_Open(TIMER0, TIMER_TOGGLE_MODE, 2);
    TIMER_Start(TIMER0);
}
