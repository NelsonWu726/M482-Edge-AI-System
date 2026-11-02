#include "NuMicro.h"

//--- hclk.c
void init_HCLK_HXT_192M(void);
void init_HCLK_HXT(void);

void init_HCLK_HXT_192M(void)
{
	SYS_UnlockReg();
	PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);
	CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
	CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
	CLK_SetCoreClock(FREQ_192MHZ);
	CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);
	SystemCoreClockUpdate();
	SYS_LockReg();
}

void init_HCLK_HXT(void)
{
	SYS_UnlockReg();
	PF->MODE &= ~(GPIO_MODE_MODE2_Msk | GPIO_MODE_MODE3_Msk);
	CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
	CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);
	CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);
	SystemCoreClockUpdate();
	SYS_LockReg();
}
