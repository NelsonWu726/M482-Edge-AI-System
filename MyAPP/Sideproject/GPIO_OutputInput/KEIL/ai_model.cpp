#include <new>
#include <stdint.h>
#include <stddef.h>

#include "tensorflow/lite/schema/schema_generated.h"


#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"


/* =========================================================
   Model Data
   ========================================================= */

extern "C"
{
    extern const unsigned char g_occupancy_model_data[];
    extern const unsigned int g_occupancy_model_data_len;
}


/* =========================================================
   Operator Resolver
   ========================================================= */

/*
 * This model requires 6 operators:
 *
 * 1. EXPAND_DIMS
 * 2. CONV_2D
 * 3. RESHAPE
 * 4. MEAN
 * 5. FULLY_CONNECTED
 * 6. LOGISTIC
 */

typedef tflite::MicroMutableOpResolver<6>
    occupancy_resolver_t;


/* =========================================================
   Resolver Static Memory
   ========================================================= */

/*
 * Do not create a global C++ object.
 *
 * Use placement new
 * to avoid running constructors before main().
 */

alignas(occupancy_resolver_t)
static unsigned char g_resolver_memory[
    sizeof(occupancy_resolver_t)
];

static occupancy_resolver_t *g_resolver = nullptr;


/* =========================================================
   Tensor Arena
   ========================================================= */

/*
 * When TFLite Micro runs the model,
 * all intermediate tensors / buffers
 * are allocated from this RAM region.
 *
 * Currently allocate 24 KB.
 *
 * If AllocateTensors() fails,
 * increase the arena size to 32 KB next.
 */

static constexpr size_t kTensorArenaSize =
    24 * 1024;

alignas(16)
static uint8_t g_tensor_arena[
    kTensorArenaSize
];


/* =========================================================
   MicroInterpreter Static Memory
   ========================================================= */

/*
 * MicroInterpreter also does not use the heap.
 *
 * Use placement new here as well.
 */

alignas(tflite::MicroInterpreter)
static unsigned char g_interpreter_memory[
    sizeof(tflite::MicroInterpreter)
];

static tflite::MicroInterpreter *g_interpreter =
    nullptr;


/* =========================================================
   Model / Tensor Pointers
   ========================================================= */

static const tflite::Model *g_model =
    nullptr;

static TfLiteTensor *g_input =
    nullptr;

static TfLiteTensor *g_output =
    nullptr;


/* =========================================================
   State
   ========================================================= */

static bool g_ai_initialized =
    false;


/* =========================================================
   AI Model Init / Test
   ========================================================= */

/*
 * Return:
 *
 *   0  = success
 *
 *  -1  = EXPAND_DIMS registration failed
 *  -2  = CONV_2D registration failed
 *  -3  = RESHAPE registration failed
 *  -4  = MEAN registration failed
 *  -5  = FULLY_CONNECTED registration failed
 *  -6  = LOGISTIC registration failed
 *  -7  = model data invalid
 *  -8  = TFLite schema version mismatch
 *  -9  = model pointer invalid
 * -10  = AllocateTensors failed
 * -11  = input tensor invalid
 * -12  = output tensor invalid
 * -13  = input tensor is not INT8
 * -14  = output tensor is not INT8
 * -15  = input shape unexpected
 */
extern "C" int ai_model_test(void)
{
    /* =====================================================
       Already Initialized
       ===================================================== */

    if(g_ai_initialized)
    {
        return 0;
    }


    /* =====================================================
       Step 1: Check Model Data
       ===================================================== */

    if(g_occupancy_model_data_len == 0)
    {
        return -7;
    }


    /*
     * Verify that the model_data in Flash
     * can actually be read by the CPU.
     */

    volatile unsigned char first_byte;

    first_byte =
        g_occupancy_model_data[0];

    (void)first_byte;


    /* =====================================================
       Step 2: Parse TFLite FlatBuffer
       ===================================================== */

    g_model =
        tflite::GetModel(
            g_occupancy_model_data
        );


    if(g_model == nullptr)
    {
        return -9;
    }


    /* =====================================================
       Step 3: Check Schema Version
       ===================================================== */

    if(
        g_model->version() !=
        TFLITE_SCHEMA_VERSION
    )
    {
        return -8;
    }


    /* =====================================================
       Step 4: Create Resolver
       ===================================================== */

    g_resolver =
        new (g_resolver_memory)
        occupancy_resolver_t();


    /* =========================
       EXPAND_DIMS
       ========================= */

    if(
        g_resolver->AddExpandDims()
        != kTfLiteOk
    )
    {
        return -1;
    }


    /* =========================
       CONV_2D
       ========================= */

    if(
        g_resolver->AddConv2D()
        != kTfLiteOk
    )
    {
        return -2;
    }


    /* =========================
       RESHAPE
       ========================= */

    if(
        g_resolver->AddReshape()
        != kTfLiteOk
    )
    {
        return -3;
    }


    /* =========================
       MEAN
       ========================= */

    if(
        g_resolver->AddMean()
        != kTfLiteOk
    )
    {
        return -4;
    }


    /* =========================
       FULLY_CONNECTED
       ========================= */

    if(
        g_resolver->AddFullyConnected()
        != kTfLiteOk
    )
    {
        return -5;
    }


    /* =========================
       LOGISTIC
       ========================= */

    if(
        g_resolver->AddLogistic()
        != kTfLiteOk
    )
    {
        return -6;
    }


    /* =====================================================
       Step 5: Create MicroInterpreter
       ===================================================== */

    g_interpreter =
        new (g_interpreter_memory)
        tflite::MicroInterpreter(
            g_model,
            *g_resolver,
            g_tensor_arena,
            kTensorArenaSize
        );


    if(g_interpreter == nullptr)
    {
        return -9;
    }


    /* =====================================================
       Step 6: Allocate Tensors
       ===================================================== */

    TfLiteStatus allocate_status =
        g_interpreter->AllocateTensors();


    if(allocate_status != kTfLiteOk)
    {
        return -10;
    }


    /* =====================================================
       Step 7: Get Input Tensor
       ===================================================== */

    g_input =
        g_interpreter->input(0);


    if(g_input == nullptr)
    {
        return -11;
    }


    /* =====================================================
       Step 8: Get Output Tensor
       ===================================================== */

    g_output =
        g_interpreter->output(0);


    if(g_output == nullptr)
    {
        return -12;
    }


    /* =====================================================
       Step 9: Check Input Type
       ===================================================== */

    /*
     * The deployment model is an INT8 model.
     */

    if(g_input->type != kTfLiteInt8)
    {
        return -13;
    }


    /* =====================================================
       Step 10: Check Output Type
       ===================================================== */

    if(g_output->type != kTfLiteInt8)
    {
        return -14;
    }


    /* =====================================================
       Step 11: Check Input Shape
       ===================================================== */

    /*
     * Expected:
     *
     * [1, 10, 2]
     *
     * batch = 1
     * time  = 10
     * feature = 2
     *
     * feature:
     *   0 = Temperature
     *   1 = Light
     */

    if(g_input->dims == nullptr)
    {
        return -15;
    }


    if(g_input->dims->size != 3)
    {
        return -15;
    }


    if(g_input->dims->data[0] != 1)
    {
        return -15;
    }


    if(g_input->dims->data[1] != 10)
    {
        return -15;
    }


    if(g_input->dims->data[2] != 2)
    {
        return -15;
    }


    /* =====================================================
       Initialization Complete
       ===================================================== */

    g_ai_initialized =
        true;


    return 0;
}


/* =========================================================
   AI Ready Check
   ========================================================= */

/*
 * inference_task.c can later use this function
 * to check whether the model is initialized.
 */

extern "C" int ai_model_is_ready(void)
{
    if(g_ai_initialized)
    {
        return 1;
    }

    return 0;
}


/* =========================================================
   Input Quantization Information
   ========================================================= */

/*
 * Keep these two functions for now.
 *
 * In the next step, inference_task.c
 * will use them to convert float Temperature / Lux
 * values to INT8.
 */

extern "C" float ai_model_input_scale(void)
{
    if(g_input == nullptr)
    {
        return 0.0f;
    }

    return g_input->params.scale;
}


extern "C" int ai_model_input_zero_point(void)
{
    if(g_input == nullptr)
    {
        return 0;
    }

    return g_input->params.zero_point;
}


/* =========================================================
   Output Quantization Information
   ========================================================= */

extern "C" float ai_model_output_scale(void)
{
    if(g_output == nullptr)
    {
        return 0.0f;
    }

    return g_output->params.scale;
}


extern "C" int ai_model_output_zero_point(void)
{
    if(g_output == nullptr)
    {
        return 0;
    }

    return g_output->params.zero_point;
}

/* =========================================================
   Real AI Inference
   ========================================================= */

/*
 * Input:
 *
 * input_20 =
 *
 * [0]  temp0
 * [1]  lux0
 * [2]  temp1
 * [3]  lux1
 * ...
 * [18] temp9
 * [19] lux9
 *
 *
 * Output:
 *
 * ai_result:
 *
 * 0 = EMPTY
 * 1 = OCCUPIED
 *
 *
 * Return:
 *
 *  0 = success
 * -1 = AI not initialized
 * -2 = invalid parameter
 * -3 = input tensor invalid
 * -4 = Invoke failed
 * -5 = output tensor invalid
 */

extern "C" int ai_model_predict(
    const float *input_20,
    uint8_t *ai_result
)
{
    int i;

    /*
     * preprocessing.json
     */
    static const float kTempMean =
        25.45401382446289f;

    static const float kTempStd =
        0.35133326053619385f;

    static const float kLuxMean =
        25.445058822631836f;

    static const float kLuxStd =
        51.00874710083008f;

    static const float kThreshold =
        0.5f;


    /* =========================
       Check State
       ========================= */

    if(!g_ai_initialized)
    {
        return -1;
    }


    if(
        (input_20 == nullptr) ||
        (ai_result == nullptr)
    )
    {
        return -2;
    }


    if(
        (g_input == nullptr) ||
        (g_input->data.int8 == nullptr)
    )
    {
        return -3;
    }


    /* =====================================================
       Fill Input Tensor
       ===================================================== */

    for(i = 0; i < 10; i++)
    {
        float temperature;
        float lux;

        float temp_normalized;
        float lux_normalized;

        float temp_quantized_float;
        float lux_quantized_float;

        int32_t temp_quantized;
        int32_t lux_quantized;


        /* =========================
           Original Sensor Data
           ========================= */

        temperature =
            input_20[
                (i * 2) + 0
            ];

        lux =
            input_20[
                (i * 2) + 1
            ];


        /* =========================
           Standardization
           ========================= */

        /*
         * Must be identical to Python:
         *
         * x = (x - mean) / std
         */

        temp_normalized =
            (temperature - kTempMean)
            /
            kTempStd;


        lux_normalized =
            (lux - kLuxMean)
            /
            kLuxStd;


        /* =========================
           INT8 Quantization
           ========================= */

        /*
         * q =
         * round(
         *     real_value / scale
         * ) + zero_point
         */

        temp_quantized_float =
            temp_normalized
            /
            g_input->params.scale;


        lux_quantized_float =
            lux_normalized
            /
            g_input->params.scale;


        /*
         * Manual rounding.
         *
         * Avoid requiring roundf().
         */

        if(temp_quantized_float >= 0.0f)
        {
            temp_quantized =
                (int32_t)(
                    temp_quantized_float
                    + 0.5f
                );
        }
        else
        {
            temp_quantized =
                (int32_t)(
                    temp_quantized_float
                    - 0.5f
                );
        }


        if(lux_quantized_float >= 0.0f)
        {
            lux_quantized =
                (int32_t)(
                    lux_quantized_float
                    + 0.5f
                );
        }
        else
        {
            lux_quantized =
                (int32_t)(
                    lux_quantized_float
                    - 0.5f
                );
        }


        temp_quantized +=
            g_input->params.zero_point;


        lux_quantized +=
            g_input->params.zero_point;


        /* =========================
           Clamp INT8
           ========================= */

        if(temp_quantized > 127)
        {
            temp_quantized = 127;
        }
        else if(temp_quantized < -128)
        {
            temp_quantized = -128;
        }


        if(lux_quantized > 127)
        {
            lux_quantized = 127;
        }
        else if(lux_quantized < -128)
        {
            lux_quantized = -128;
        }


        /* =========================
           Write Tensor
           ========================= */

        /*
         * Tensor layout:
         *
         * [1,10,2]
         *
         * [
         *   [temp0, lux0],
         *   [temp1, lux1],
         *   ...
         * ]
         */

        g_input->data.int8[
            (i * 2) + 0
        ] =
            (int8_t)temp_quantized;


        g_input->data.int8[
            (i * 2) + 1
        ] =
            (int8_t)lux_quantized;
    }


    /* =====================================================
       Run Neural Network
       ===================================================== */

    if(
        g_interpreter->Invoke()
        != kTfLiteOk
    )
    {
        return -4;
    }


    /* =====================================================
       Read Output
       ===================================================== */

    if(
        (g_output == nullptr) ||
        (g_output->data.int8 == nullptr)
    )
    {
        return -5;
    }


    /*
     * INT8 output → float probability
     *
     * real =
     * scale * (q - zero_point)
     */

    int32_t output_q =
        (int32_t)
        g_output->data.int8[0];


    float probability =
        g_output->params.scale
        *
        (
            output_q -
            g_output->params.zero_point
        );


    /* =========================
       Threshold = 0.5
       ========================= */

    if(probability >= kThreshold)
    {
        *ai_result = 1;
    }
    else
    {
        *ai_result = 0;
    }


    return 0;
}