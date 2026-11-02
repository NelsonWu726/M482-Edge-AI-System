#include <stdint.h>


/* =========================================================
   Peak Detection Configuration
   ========================================================= */

/*
 * Number of heart-beat intervals used for BPM smoothing.
 *
 * We store 5 previous valley positions.
 * When the 6th valley arrives:
 *
 *     current valley - valley 5 beats ago
 *
 * gives the sample count across 5 heart beats.
 */
#define PEAK_HISTORY_SIZE    5


/* =========================================================
   Internal State
   ========================================================= */

/*
 * Minimum signal change required to confirm
 * peak / valley direction change.
 */
static uint32_t g_peak_level = 200;


/*
 * Current sample number.
 */
static uint32_t g_sample_index = 0;


/*
 * Direction:
 *
 * 1 = signal currently rising
 * 0 = signal currently falling
 */
static uint8_t g_direction = 1;


/*
 * Current local maximum / minimum.
 */
static uint32_t g_max_value = 0;

static uint32_t g_min_value = 0xFFFFFFFFUL;


/*
 * Sample positions of current max / min.
 */
static uint32_t g_max_index = 0;

static uint32_t g_min_index = 0;


/*
 * Previous valley positions.
 *
 * Used to calculate the sample distance
 * across 5 heart beats.
 */
static uint32_t g_valley_index[PEAK_HISTORY_SIZE];


/*
 * Number of valid valleys collected.
 */
static uint8_t g_valley_count = 0;


/*
 * Ring-buffer write position.
 */
static uint8_t g_valley_slot = 0;


/* =========================================================
   Initialize Peak Detector
   ========================================================= */

void peak_init(uint32_t level)
{
    uint8_t i;


    g_peak_level = level;

    g_sample_index = 0;

    g_direction = 1;

    g_max_value = 0;

    g_min_value = 0xFFFFFFFFUL;

    g_max_index = 0;

    g_min_index = 0;

    g_valley_count = 0;

    g_valley_slot = 0;


    for(i = 0; i < PEAK_HISTORY_SIZE; i++)
    {
        g_valley_index[i] = 0;
    }
}


/* =========================================================
   Peak / Valley Detector

   Input:
       py = current IR sample

   Return:
       0
           No valid BPM interval yet.

       >0
           Number of samples across 5 heart beats.

   Example:

       If sampling frequency = 100 Hz

       5 heart beats at 60 BPM:
           dd ~= 500 samples

       BPM =
           60 * 100 * 5 / dd

           = 30000 / dd
   ========================================================= */

uint32_t peak(uint32_t py)
{
    uint32_t dd;


    dd = 0;

    g_sample_index++;


    /* =====================================================
       Signal Rising
       ===================================================== */

    if(g_direction == 1)
    {
        /*
         * Still going upward.
         */
        if(py > g_max_value)
        {
            g_max_value = py;

            g_max_index = g_sample_index;
        }

        /*
         * Signal dropped enough from maximum:
         * peak has been confirmed.
         */
        else if(
            (g_max_value > py) &&
            ((g_max_value - py) > g_peak_level)
        )
        {
            g_direction = 0;


            /*
             * Start searching for valley.
             */
            g_min_value = py;

            g_min_index = g_sample_index;
        }
    }


    /* =====================================================
       Signal Falling
       ===================================================== */

    else
    {
        /*
         * Still going downward.
         */
        if(py < g_min_value)
        {
            g_min_value = py;

            g_min_index = g_sample_index;
        }

        /*
         * Signal rose enough from minimum:
         * valley has been confirmed.
         */
        else if(
            (py > g_min_value) &&
            ((py - g_min_value) > g_peak_level)
        )
        {
            g_direction = 1;


            /*
             * Start searching for next peak.
             */
            g_max_value = py;

            g_max_index = g_sample_index;


            /* =============================================
               Initial Warm-up

               Need 5 previous valley positions before
               calculating a 5-beat interval.
               ============================================= */

            if(g_valley_count < PEAK_HISTORY_SIZE)
            {
                g_valley_index[g_valley_slot] =
                    g_min_index;


                g_valley_slot++;


                if(g_valley_slot >= PEAK_HISTORY_SIZE)
                {
                    g_valley_slot = 0;
                }


                g_valley_count++;


                return 0;
            }


            /* =============================================
               Calculate distance across 5 beats

               g_valley_slot currently points to the
               oldest valley in the ring buffer.
               ============================================= */

            dd =
                g_min_index -
                g_valley_index[g_valley_slot];


            /*
             * Replace oldest valley with newest valley.
             */
            g_valley_index[g_valley_slot] =
                g_min_index;


            g_valley_slot++;


            if(g_valley_slot >= PEAK_HISTORY_SIZE)
            {
                g_valley_slot = 0;
            }


            return dd;
        }
    }


    return 0;
}