#include "NuMicro.h"

/* =========================
   ADC Data
   ========================= */

static volatile uint16_t adc_value[19];

static volatile uint8_t adc_ready = 0;

static volatile uint8_t adc_done = 0;

#define ADC_DONE_TR       0x01
#define ADC_DONE_VR       0x02
#define ADC_DONE_BANDGAP  0x04
#define ADC_DONE_ALL      0x07


/* =========================
   Temperature lookup table
   ========================= */

static const uint16_t temp_table[] = {
     539,  564,  590,  617,  645,  673,  702,  732,  763,  794,
     826,  859,  893,  927,  962,  997, 1034, 1070, 1108, 1146,
    1184, 1223, 1263, 1302, 1343, 1383, 1424, 1465, 1506, 1548,
    1590, 1632, 1674, 1716, 1758, 1800, 1842, 1883, 1925, 1967,
    2008, 2049, 2090, 2130, 2171, 2211, 2250, 2289, 2328, 2366,
    2404, 2441, 2478, 2514, 2550, 2585, 2620, 2654, 2688, 2721,
    2753
};


/* =========================
   Function declaration
   ========================= */

void init_adc(void);
void adc_start(void);

uint8_t adc_is_ready(void);

uint16_t adc_get_tr(void);
uint16_t adc_get_vr(void);
uint16_t adc_get_bandgap(void);

float adc_get_temperature(void);


/* =========================
   ADC INIT
   ========================= */

void init_adc(void)
{
    SYS_UnlockReg();

    /* EADC clock */
    CLK_EnableModuleClock(EADC_MODULE);

    /*
       EADC clock
       PCLK / 8
    */
    CLK_SetModuleClock(
        EADC_MODULE,
        0,
        CLK_CLKDIV0_EADC(8)
    );


    /* =========================
       ADC PIN
       ========================= */

    /* PB13 -> EADC CH13
       Variable Resistor
    */
    SYS->GPB_MFPH =
        (SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB13MFP_Msk)
        | SYS_GPB_MFPH_PB13MFP_EADC0_CH13;


    /* PB12 -> EADC CH12
       Thermistor
    */
		SYS->GPB_MFPH =
				(SYS->GPB_MFPH & ~SYS_GPB_MFPH_PB12MFP_Msk)
				| SYS_GPB_MFPH_PB12MFP_EADC0_CH12;

    /* Disable digital input path */
    GPIO_DISABLE_DIGITAL_PATH(
        PB,
        BIT12 | BIT13
    );

    SYS_LockReg();


    /* =========================
       ADC MODE
       ========================= */

    EADC_Open(
        EADC,
        EADC_CTL_DIFFEN_SINGLE_END
    );


    /* =========================
       Sample Module
       ========================= */

    /* SM12 -> CH12 Thermistor */
    EADC_ConfigSampleModule(
        EADC,
        12,
        EADC_SOFTWARE_TRIGGER,
        12
    );

    /* SM13 -> CH13 VR */
    EADC_ConfigSampleModule(
        EADC,
        13,
        EADC_SOFTWARE_TRIGGER,
        13
    );

    /* SM16 -> Internal Bandgap */
    EADC_ConfigSampleModule(
        EADC,
        16,
        EADC_SOFTWARE_TRIGGER,
        16
    );


    /* Extend sampling time */
    EADC_SetExtendSampleTime(
        EADC,
        12,
        32
    );

    EADC_SetExtendSampleTime(
        EADC,
        13,
        32
    );

    EADC_SetExtendSampleTime(
        EADC,
        16,
        32
    );


    /* =========================
       ADC Interrupt
       ========================= */

    /* Disable IRQ while configuring */
    NVIC_DisableIRQ(EADC00_IRQn);

    /* Clear old ADC flag */
    EADC_CLR_INT_FLAG(
        EADC,
        EADC_STATUS2_ADIF0_Msk
    );

    NVIC_ClearPendingIRQ(EADC00_IRQn);


    /* Enable ADINT0 */
    EADC_ENABLE_INT(
        EADC,
        BIT0
    );

    EADC_ENABLE_SAMPLE_MODULE_INT(
        EADC,
        0,
        BIT12 | BIT13 | BIT16
    );


    NVIC_SetPriority(
        EADC00_IRQn,
        5
    );

    NVIC_EnableIRQ(EADC00_IRQn);
}


/* =========================
   EADC ISR
   ========================= */

void EADC00_IRQHandler(void)
{
    uint32_t valid;

    valid = EADC_GET_DATA_VALID_FLAG(
        EADC,
        BIT12 | BIT13 | BIT16
    );


    /* Thermistor */
    if(valid & BIT12)
    {
        adc_value[12] =
            EADC_GET_CONV_DATA(EADC, 12);

        adc_done |= ADC_DONE_TR;
    }


    /* Variable Resistor */
    if(valid & BIT13)
    {
        adc_value[13] =
            EADC_GET_CONV_DATA(EADC, 13);

        adc_done |= ADC_DONE_VR;
    }


    /* Bandgap */
    if(valid & BIT16)
    {
        adc_value[16] =
            EADC_GET_CONV_DATA(EADC, 16);

        adc_done |= ADC_DONE_BANDGAP;
    }


    /* Clear ADINT0 flag */
    EADC_CLR_INT_FLAG(
        EADC,
        EADC_STATUS2_ADIF0_Msk
    );


    if(adc_done == ADC_DONE_ALL)
    {
        adc_ready = 1;
    }
}


/* =========================
   Start ADC
   ========================= */

void adc_start(void)
{
    adc_ready = 0;
    adc_done  = 0;

    EADC_START_CONV(
        EADC,
        BIT12 | BIT13 | BIT16
    );
}


/* =========================
   ADC status
   ========================= */

uint8_t adc_is_ready(void)
{
    return adc_ready;
}


/* =========================
   Get Thermistor
   ========================= */

uint16_t adc_get_tr(void)
{
    return adc_value[12];
}


/* =========================
   Raw -> Temperature
   ========================= */

static float adc_raw_to_temp(uint16_t n)
{
    int i;

    uint16_t a;
    uint16_t b;

    float c;
    float d;

    for(i = 0; i < 61; i++)
    {
        if(n < temp_table[i])
        {
            break;
        }
    }

    if(i == 0)
    {
        return -1.0f;
    }

    if(i == 61)
    {
        return -1.0f;
    }

    a = temp_table[i - 1];
    b = temp_table[i];

    c = n - a;
    d = b - a;

    return (i - 1) + (c / d);
}


/* =========================
   Get VR
   ========================= */

uint16_t adc_get_vr(void)
{
    return adc_value[13];
}


/* =========================
   Get Temperature
   ========================= */

float adc_get_temperature(void)
{
    return adc_raw_to_temp(
        adc_value[12]
    );
}


/* =========================
   Get Bandgap
   ========================= */

uint16_t adc_get_bandgap(void)
{
    return adc_value[16];
}
