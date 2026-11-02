#include "NuMicro.h"

/* uart.c */
void init_UART1(void);


void init_UART1(void)
{
    SYS_UnlockReg();

    /* 1. CLK */
    CLK_EnableModuleClock(UART1_MODULE);

    CLK_SetModuleClock(
        UART1_MODULE,
        CLK_CLKSEL1_UART1SEL_HXT,
        CLK_CLKDIV0_UART1(1)
    );

    /* 2. PIN */
    SYS->GPB_MFPL =
        (SYS->GPB_MFPL & ~SYS_GPB_MFPL_PB6MFP_Msk)
        | SYS_GPB_MFPL_PB6MFP_UART1_RXD;

    SYS->GPB_MFPL =
        (SYS->GPB_MFPL & ~SYS_GPB_MFPL_PB7MFP_Msk)
        | SYS_GPB_MFPL_PB7MFP_UART1_TXD;

    SYS_LockReg();

    /* 3. OPEN */
    UART_Open(UART1, 115200);

    /* 4. UART RX interrupt */
    UART_EnableInt(
        UART1,
        UART_INTEN_RDAIEN_Msk
    );

    /* 5. NVIC */
    NVIC_EnableIRQ(UART1_IRQn);
}
