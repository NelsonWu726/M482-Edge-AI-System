#include "NuMicro.h"

__asm(".global __use_no_semihosting\n");

/* app.c */
void app_start(void);

int32_t main(void)
{
    app_start();

    for(;;)
    {
    }
}