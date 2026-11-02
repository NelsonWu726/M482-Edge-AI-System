#include <stdint.h>
#include <math.h>


/* =========================================================
   SpO2 Algorithm Configuration
   ========================================================= */

/*
 * Current MAX30102 effective RAW sample rate:
 *
 *     about 100 samples/sec
 *
 * Use 400 RAW samples:
 *
 *     400 / 100 Hz = about 4 seconds
 *
 * SpO2 is calculated once every complete window.
 */
#define SPO2_WINDOW_SIZE             400UL


/*
 * Minimum DC level.
 *
 * Your current measurements:
 *
 * No finger:
 *     RED / IR usually only hundreds
 *
 * Finger:
 *     RED / IR usually tens of thousands
 *
 * Finger detection is also performed in max30102_task.c,
 * but this provides another safety check.
 */
#define SPO2_MIN_DC_LEVEL           5000.0f


/*
 * Minimum pulsatile AC RMS amplitude.
 *
 * Very small AC means that there is probably not enough
 * heartbeat information for a meaningful SpO2 estimate.
 */
#define SPO2_MIN_AC_LEVEL             20.0f


/*
 * Valid ratio-of-ratios range.
 *
 * Values far outside this range normally indicate:
 *
 *     motion
 *     bad finger contact
 *     weak PPG
 *     invalid signal
 */
#define SPO2_MIN_RATIO                 0.20f
#define SPO2_MAX_RATIO                 1.80f


/*
 * IR perfusion / signal variation limits.
 *
 * AC / DC too small:
 *     pulse waveform too weak
 *
 * AC / DC too large:
 *     likely motion / unstable contact
 */
#define SPO2_MIN_PERFUSION             0.001f
#define SPO2_MAX_PERFUSION             0.300f


/*
 * Output sanity range.
 *
 * This is NOT a medical validation range.
 * It is only used to reject obviously unusable estimates.
 */
#define SPO2_MIN_OUTPUT               70.0f
#define SPO2_MAX_OUTPUT              100.0f


/* =========================================================
   Internal Accumulators
   ========================================================= */

/*
 * 64-bit accumulators are used because:
 *
 * MAX30102 is effectively an 18-bit ADC.
 *
 * Squaring a sample can become very large.
 */
static uint64_t g_red_sum = 0;

static uint64_t g_ir_sum = 0;


static uint64_t g_red_square_sum = 0;

static uint64_t g_ir_square_sum = 0;


static uint32_t g_sample_count = 0;


/* =========================================================
   Current Result
   ========================================================= */

static uint32_t g_spo2_value = 0;

static uint8_t g_spo2_valid = 0;


/* =========================================================
   Debug Variables

   These can be added directly to Keil Watch.
   ========================================================= */

/*
 * Number of samples currently collected
 * in the active SpO2 window.
 */
volatile uint32_t spo2_sample_count_debug = 0;


/*
 * DC values calculated from latest window.
 */
volatile float spo2_red_dc_debug = 0.0f;

volatile float spo2_ir_dc_debug = 0.0f;


/*
 * AC RMS values calculated from latest window.
 */
volatile float spo2_red_ac_debug = 0.0f;

volatile float spo2_ir_ac_debug = 0.0f;


/*
 * Ratio-of-ratios:
 *
 *          RED_AC / RED_DC
 * R = -------------------------
 *          IR_AC / IR_DC
 */
volatile float spo2_ratio_debug = 0.0f;


/*
 * IR AC/DC ratio.
 *
 * Useful for checking signal quality.
 */
volatile float spo2_perfusion_debug = 0.0f;


/*
 * Raw floating-point SpO2 result
 * before conversion to integer.
 */
volatile float spo2_float_debug = 0.0f;


/*
 * Latest integer SpO2 result.
 */
volatile uint32_t spo2_value_debug = 0;


/*
 * 1 = current SpO2 is valid
 * 0 = invalid
 */
volatile uint8_t spo2_valid_debug = 0;


/*
 * Number of successfully calculated
 * SpO2 windows.
 */
volatile uint32_t spo2_update_count_debug = 0;


/*
 * Number of rejected windows.
 */
volatile uint32_t spo2_reject_count_debug = 0;


/* =========================================================
   Clear Current Sample Window

   Does NOT erase the latest SpO2 result.
   ========================================================= */

static void spo2_clear_window(void)
{
    g_red_sum = 0;

    g_ir_sum = 0;


    g_red_square_sum = 0;

    g_ir_square_sum = 0;


    g_sample_count = 0;


    spo2_sample_count_debug = 0;
}


/* =========================================================
   Initialize SpO2 Algorithm

   Call once during task initialization.
   ========================================================= */

void spo2_init(void)
{
    spo2_clear_window();


    g_spo2_value = 0;

    g_spo2_valid = 0;


    spo2_red_dc_debug = 0.0f;

    spo2_ir_dc_debug = 0.0f;


    spo2_red_ac_debug = 0.0f;

    spo2_ir_ac_debug = 0.0f;


    spo2_ratio_debug = 0.0f;

    spo2_perfusion_debug = 0.0f;

    spo2_float_debug = 0.0f;


    spo2_value_debug = 0;

    spo2_valid_debug = 0;


    spo2_update_count_debug = 0;

    spo2_reject_count_debug = 0;
}


/* =========================================================
   Reset Current Measurement Session

   Use this when:

       finger removed
       AI becomes EMPTY
       new finger measurement starts

   Debug statistics are preserved.
   ========================================================= */

void spo2_reset(void)
{
    spo2_clear_window();


    g_spo2_value = 0;

    g_spo2_valid = 0;


    spo2_red_dc_debug = 0.0f;

    spo2_ir_dc_debug = 0.0f;


    spo2_red_ac_debug = 0.0f;

    spo2_ir_ac_debug = 0.0f;


    spo2_ratio_debug = 0.0f;

    spo2_perfusion_debug = 0.0f;

    spo2_float_debug = 0.0f;


    spo2_value_debug = 0;

    spo2_valid_debug = 0;
}


/* =========================================================
   Calculate SpO2 From One Complete Window

   Return:
       1 = calculation performed and result valid
       0 = calculation performed but signal invalid
   ========================================================= */

static uint8_t spo2_calculate_window(void)
{
    float red_dc;

    float ir_dc;


    float red_variance;

    float ir_variance;


    float red_ac;

    float ir_ac;


    float red_normalized;

    float ir_normalized;


    float ratio;

    float perfusion;


    float spo2;


    uint64_t red_mean_square_term;

    uint64_t ir_mean_square_term;


    uint64_t red_variance_acc;

    uint64_t ir_variance_acc;


    /* =====================================================
       Safety Check
       ===================================================== */

    if(g_sample_count == 0)
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       DC Component

       DC = average signal level
       ===================================================== */

    red_dc =
        (float)g_red_sum /
        (float)g_sample_count;


    ir_dc =
        (float)g_ir_sum /
        (float)g_sample_count;


    spo2_red_dc_debug =
        red_dc;


    spo2_ir_dc_debug =
        ir_dc;


    /* =====================================================
       Finger / DC Validation
       ===================================================== */

    if(
        (red_dc < SPO2_MIN_DC_LEVEL) ||
        (ir_dc < SPO2_MIN_DC_LEVEL)
    )
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       Calculate Variance

       variance =
           mean(x^2) - mean(x)^2

       Integer accumulation is used first to reduce
       floating-point cancellation error.
       ===================================================== */

    red_mean_square_term =
        (g_red_sum * g_red_sum) /
        g_sample_count;


    ir_mean_square_term =
        (g_ir_sum * g_ir_sum) /
        g_sample_count;


    if(g_red_square_sum > red_mean_square_term)
    {
        red_variance_acc =
            g_red_square_sum -
            red_mean_square_term;
    }
    else
    {
        red_variance_acc = 0;
    }


    if(g_ir_square_sum > ir_mean_square_term)
    {
        ir_variance_acc =
            g_ir_square_sum -
            ir_mean_square_term;
    }
    else
    {
        ir_variance_acc = 0;
    }


    red_variance =
        (float)red_variance_acc /
        (float)g_sample_count;


    ir_variance =
        (float)ir_variance_acc /
        (float)g_sample_count;


    /* =====================================================
       AC Component

       AC RMS = sqrt(variance)
       ===================================================== */

    red_ac =
        sqrtf(
            red_variance
        );


    ir_ac =
        sqrtf(
            ir_variance
        );


    spo2_red_ac_debug =
        red_ac;


    spo2_ir_ac_debug =
        ir_ac;


    /* =====================================================
       AC Validation
       ===================================================== */

    if(
        (red_ac < SPO2_MIN_AC_LEVEL) ||
        (ir_ac < SPO2_MIN_AC_LEVEL)
    )
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       Normalize AC With DC

           RED normalized = RED_AC / RED_DC

           IR normalized  = IR_AC / IR_DC
       ===================================================== */

    red_normalized =
        red_ac /
        red_dc;


    ir_normalized =
        ir_ac /
        ir_dc;


    /* =====================================================
       Prevent Invalid Division
       ===================================================== */

    if(ir_normalized <= 0.0f)
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       Perfusion / Signal Quality

       For now use:

           IR_AC / IR_DC
       ===================================================== */

    perfusion =
        ir_normalized;


    spo2_perfusion_debug =
        perfusion;


    if(
        (perfusion < SPO2_MIN_PERFUSION) ||
        (perfusion > SPO2_MAX_PERFUSION)
    )
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       Ratio Of Ratios

                  RED_AC / RED_DC
       R = -----------------------------
                  IR_AC / IR_DC
       ===================================================== */

    ratio =
        red_normalized /
        ir_normalized;


    spo2_ratio_debug =
        ratio;


    /* =====================================================
       Ratio Validation
       ===================================================== */

    if(
        (ratio < SPO2_MIN_RATIO) ||
        (ratio > SPO2_MAX_RATIO)
    )
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       SpO2 Empirical Estimate

       Approximation:

       SpO2 =
           -45.060 * R^2
           +30.354 * R
           +94.845

       IMPORTANT:

       This is an empirical approximation.

       Real pulse oximeters require calibration against
       reference measurements. This project result should
       therefore be treated as an estimate, not as a
       clinical measurement.
       ===================================================== */

    spo2 =
        (-45.060f * ratio * ratio) +
        ( 30.354f * ratio) +
          94.845f;


    spo2_float_debug =
        spo2;


    /* =====================================================
       Allow Slightly >100 From Formula, Then Clamp To 100
       ===================================================== */

    if(spo2 > SPO2_MAX_OUTPUT)
    {
        spo2 =
            SPO2_MAX_OUTPUT;
    }


    /* =====================================================
       Reject Very Low / Obviously Invalid Estimate
       ===================================================== */

    if(spo2 < SPO2_MIN_OUTPUT)
    {
        g_spo2_valid = 0;

        spo2_valid_debug = 0;

        spo2_reject_count_debug++;

        return 0;
    }


    /* =====================================================
       Convert To Integer Percentage

       Example:
           97.6 -> 98
       ===================================================== */

    g_spo2_value =
        (uint32_t)(
            spo2 +
            0.5f
        );


    g_spo2_valid =
        1;


    spo2_value_debug =
        g_spo2_value;


    spo2_valid_debug =
        1;


    spo2_update_count_debug++;


    return 1;
}


/* =========================================================
   Add One RAW RED + IR Sample

   IMPORTANT:

   Feed RAW samples here BEFORE network decimation.

   Current system:
       RAW approximately 100 Hz

   Return:
       0
           Window not finished yet
           OR completed window was invalid

       1
           New VALID SpO2 value has been calculated
   ========================================================= */

uint8_t spo2_update(
    uint32_t red,
    uint32_t ir
)
{
    uint8_t result;


    result = 0;


    /* =====================================================
       Accumulate RED
       ===================================================== */

    g_red_sum +=
        (uint64_t)red;


    g_red_square_sum +=
        (uint64_t)red *
        (uint64_t)red;


    /* =====================================================
       Accumulate IR
       ===================================================== */

    g_ir_sum +=
        (uint64_t)ir;


    g_ir_square_sum +=
        (uint64_t)ir *
        (uint64_t)ir;


    /* =====================================================
       Sample Counter
       ===================================================== */

    g_sample_count++;


    spo2_sample_count_debug =
        g_sample_count;


    /* =====================================================
       Complete Window
       ===================================================== */

    if(
        g_sample_count >=
        SPO2_WINDOW_SIZE
    )
    {
        result =
            spo2_calculate_window();


        /*
         * Start collecting a new independent window.
         */
        spo2_clear_window();
    }


    return result;
}


/* =========================================================
   Get Latest SpO2

   Return:
       0 = no valid value
       1 = valid SpO2 available
   ========================================================= */

uint8_t spo2_get_value(
    uint32_t *spo2
)
{
    if(spo2 == 0)
    {
        return 0;
    }


    if(!g_spo2_valid)
    {
        return 0;
    }


    *spo2 =
        g_spo2_value;


    return 1;
}


/* =========================================================
   Check Whether Current SpO2 Is Valid
   ========================================================= */

uint8_t spo2_is_valid(void)
{
    return g_spo2_valid;
}


/* =========================================================
   Get Current Ratio

   Mainly for debugging / calibration.
   ========================================================= */

float spo2_get_ratio(void)
{
    return spo2_ratio_debug;
}