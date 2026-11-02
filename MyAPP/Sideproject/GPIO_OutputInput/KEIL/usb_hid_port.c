#include "NuMicro.h"
#include "hid_mousekeyboard.h"

/* =========================================================
   USB HID Initialization
   HID Mouse + HID Keyboard
   ========================================================= */

void usb_hid_init(void)
{
    /* -----------------------------------------------------
       1. Unlock protected registers
       ----------------------------------------------------- */

    SYS_UnlockReg();


    /* -----------------------------------------------------
       2. Configure USB Clock

       PLL = 192 MHz (initialized by hclk.c)
       USB = 192 MHz / 4 = 48 MHz
       ----------------------------------------------------- */

    CLK->CLKSEL0 |= CLK_CLKSEL0_USBSEL_Msk;

    CLK->CLKDIV0 =
        (CLK->CLKDIV0 & ~CLK_CLKDIV0_USBDIV_Msk)
        | CLK_CLKDIV0_USB(4);


    /* -----------------------------------------------------
       3. Enable USB PHY
       ----------------------------------------------------- */

    SYS->USBPHY =
        (SYS->USBPHY & ~SYS_USBPHY_USBROLE_Msk)
        | SYS_USBPHY_USBEN_Msk
        | SYS_USBPHY_SBO_Msk;


    /* -----------------------------------------------------
       4. Enable USBD Module Clock
       ----------------------------------------------------- */

    CLK_EnableModuleClock(USBD_MODULE);

		
		/* Enable USB OTG clock */
		CLK_EnableModuleClock(OTG_MODULE);

		/* Enable USB ID pin detection */
		OTG->PHYCTL = OTG_PHYCTL_IDDETEN_Msk;

    /* -----------------------------------------------------
       5. Configure USB Pins

       PA12 = USB VBUS
       PA13 = USB D-
       PA14 = USB D+

       PA15 is left unchanged for now.
       ----------------------------------------------------- */

    PA->MODE &=
        ~(GPIO_MODE_MODE12_Msk |
          GPIO_MODE_MODE13_Msk |
          GPIO_MODE_MODE14_Msk);

    SYS->GPA_MFPH &=
        ~(SYS_GPA_MFPH_PA12MFP_Msk |
          SYS_GPA_MFPH_PA13MFP_Msk |
          SYS_GPA_MFPH_PA14MFP_Msk);

    SYS->GPA_MFPH |=
        (SYS_GPA_MFPH_PA12MFP_USB_VBUS |
         SYS_GPA_MFPH_PA13MFP_USB_D_N |
         SYS_GPA_MFPH_PA14MFP_USB_D_P);

		
		/* PA15 -> USB OTG ID */
		SYS->GPA_MFPH =
				(SYS->GPA_MFPH & ~SYS_GPA_MFPH_PA15MFP_Msk)
				| SYS_GPA_MFPH_PA15MFP_USB_OTG_ID;

		/* PA15 Input */
		GPIO_SetMode(PA, BIT15, GPIO_MODE_INPUT);
		
		/* PB0 ~ PB3 -> GPIO */
		SYS->GPB_MFPL &=
				~(SYS_GPB_MFPL_PB0MFP_Msk);

		/* PB0 ~ PB3 -> Output */
		GPIO_SetMode(
				PB,
				BIT0 ,
				GPIO_MODE_OUTPUT
		);

    /* -----------------------------------------------------
       6. Lock protected registers
       ----------------------------------------------------- */

    SYS_LockReg();


    /* -----------------------------------------------------
       7. Register USB Descriptors and HID Class Request
       ----------------------------------------------------- */

    USBD_Open(
        &gsInfo,
        HID_ClassRequest,
        NULL
    );


    /* -----------------------------------------------------
       8. Configure HID Endpoints
       ----------------------------------------------------- */

    HID_Init();


    /* -----------------------------------------------------
       9. Start USB Device
       ----------------------------------------------------- */

    USBD_Start();


    /* -----------------------------------------------------
       10. Enable USB Interrupt
       ----------------------------------------------------- */

    NVIC_EnableIRQ(USBD_IRQn);
}