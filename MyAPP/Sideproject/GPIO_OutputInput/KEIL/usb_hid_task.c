
#include "NuMicro.h"
#include "cmsis_os.h"
#include "hid_mousekeyboard.h"

/* PA15 is active-low (pressed = Low) */
#define PA15_PRESSED_LEVEL  0U

volatile uint32_t g_usb_button_press_count = 0;
volatile uint32_t g_usb_pa15_level_debug = 0xFFFFFFFF;

/* Automation start request */
static uint8_t g_hid_automation_requested = 0;


/* =========================================================
   Read PA15
   ========================================================= */
static uint8_t read_pa15(void)
{
    return ((PA->PIN & BIT15) != 0U) ? 1U : 0U;
}


/* =========================================================
   Request HID Automation
   ========================================================= */
static void HID_StartAutomation(void)
{
    g_hid_automation_requested = 1;
}


/* =========================================================
   USB HID Task
   ========================================================= */
void usb_hid_task(void const *argument)
{
    uint8_t pa15_level;
    uint8_t button_armed;

    (void)argument;

    /* If the button is held at startup, wait for its release */
    button_armed = (read_pa15() != PA15_PRESSED_LEVEL);

    while (1)
    {
        /* Read PA15 */
        pa15_level = read_pa15();

        g_usb_pa15_level_debug = pa15_level;

        /* Detect PA15 button press */
        if (button_armed &&
            pa15_level == PA15_PRESSED_LEVEL)
        {
            /* Debounce */
            osDelay(20);

            if (read_pa15() == PA15_PRESSED_LEVEL)
            {
                /* Prevent repeated triggering while held */
                button_armed = 0;

                /* Increment trigger count */
                g_usb_button_press_count++;

                /* Request HID automation */
                HID_StartAutomation();
            }
        }

        /* Re-arm the button after release */
        else if (!button_armed &&
                 pa15_level != PA15_PRESSED_LEVEL)
        {
            osDelay(20);

            if (read_pa15() != PA15_PRESSED_LEVEL)
            {
                button_armed = 1;
            }
        }

        osDelay(10);
    }
}
