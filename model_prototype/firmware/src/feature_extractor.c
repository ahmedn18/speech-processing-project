#include "feature_extractor.h"

#include <avr/pgmspace.h>
#include <limits.h>
#include <stddef.h>

#define FRAME_LENGTH 256U
#define HOP_LENGTH 128U
#define MEL_BAND_COUNT 20U
#define MFCC_COUNT 13U
#define DCT_Q_SHIFT 14U

#if MODEL_FEATURE_DIM != (MFCC_COUNT * 2U)
#error "MODEL_FEATURE_DIM must equal 2 * MFCC_COUNT"
#endif

/* Center FFT bins from the 20 mel bands used in the firmware-side approximation. */
static const int16_t k_goertzel_coeff_q14[MEL_BAND_COUNT] PROGMEM = {
    32610, 32286, 31581, 30853, 29622, 28511, 26791, 24812, 23170, 20788,
    17531, 14733, 10279, 5602, 0, -6393, -13279, -20160, -26320, -30853
};

/* DCT-II matrix (norm='ortho') in Q14 for 13 MFCCs over 20 log-mel energies. */
static const int16_t k_dct_q14[MFCC_COUNT][MEL_BAND_COUNT] PROGMEM = {
    {3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664, 3664},
    {5165, 5038, 4787, 4418, 3940, 3365, 2707, 1983, 1209, 407, -407, -1209, -1983, -2707, -3365, -3940, -4418, -4787, -5038, -5165},
    {5117, 4616, 3664, 2352, 810, -810, -2352, -3664, -4616, -5117, -5117, -4616, -3664, -2352, -810, 810, 2352, 3664, 4616, 5117},
    {5038, 3940, 1983, -407, -2707, -4418, -5165, -4787, -3365, -1209, 1209, 3365, 4787, 5165, 4418, 2707, 407, -1983, -3940, -5038},
    {4927, 3045, 0, -3045, -4927, -4927, -3045, 0, 3045, 4927, 4927, 3045, 0, -3045, -4927, -4927, -3045, 0, 3045, 4927},
    {4787, 1983, -1983, -4787, -4787, -1983, 1983, 4787, 4787, 1983, -1983, -4787, -4787, -1983, 1983, 4787, 4787, 1983, -1983, -4787},
    {4616, 810, -3664, -5117, -2352, 2352, 5117, 3664, -810, -4616, -4616, -810, 3664, 5117, 2352, -2352, -5117, -3664, 810, 4616},
    {4418, -407, -4787, -3940, 1209, 5038, 3365, -1983, -5165, -2707, 2707, 5165, 1983, -3365, -5038, -1209, 3940, 4787, 407, -4418},
    {4192, -1601, -5181, -1601, 4192, 4192, -1601, -5181, -1601, 4192, 4192, -1601, -5181, -1601, 4192, 4192, -1601, -5181, -1601, 4192},
    {3940, -2707, -4787, 1209, 5165, 407, -5038, -1983, 4418, 3365, -3365, -4418, 1983, 5038, -407, -5165, -1209, 4787, 2707, -3940},
    {3664, -3664, -3664, 3664, 3664, -3664, -3664, 3664, 3664, -3664, -3664, 3664, 3664, -3664, -3664, 3664, 3664, -3664, -3664, 3664},
    {3365, -4418, -1983, 5038, 407, -5165, 1209, 4787, -2707, -3940, 3940, 2707, -4787, -1209, 5165, -407, -5038, 1983, 4418, -3365},
    {3045, -4927, 0, 4927, -3045, -3045, 4927, 0, -4927, 3045, 3045, -4927, 0, 4927, -3045, -3045, 4927, 0, -4927, 3045}
};

static int16_t centered_sample(uint8_t sample) {
    return (int16_t) sample - 128;
}

static uint32_t integer_sqrt_u64(uint64_t value) {
    uint64_t rem = value;
    uint64_t root = 0;
    uint64_t bit = (uint64_t) 1 << 62;

    while (bit > rem) {
        bit >>= 2;
    }

    while (bit != 0U) {
        if (rem >= (root + bit)) {
            rem -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t) root;
}

static uint32_t goertzel_power(
    const uint8_t *samples,
    uint16_t length,
    uint16_t frame_start,
    int16_t coeff_q14
) {
    uint16_t n;
    int32_t s_prev = 0;
    int32_t s_prev2 = 0;

    for (n = 0; n < FRAME_LENGTH; n++) {
        uint16_t idx = (uint16_t) (frame_start + n);
        int16_t x_n = (idx < length) ? centered_sample(samples[idx]) : 0;
        int32_t s = (int32_t) x_n + (((int32_t) coeff_q14 * s_prev) >> DCT_Q_SHIFT) - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }

    {
        int64_t energy = ((int64_t) s_prev * s_prev) + ((int64_t) s_prev2 * s_prev2);
        energy -= (((int64_t) coeff_q14 * s_prev * s_prev2) >> DCT_Q_SHIFT);
        if (energy < 1) {
            return 1U;
        }
        if (energy > UINT32_MAX) {
            return UINT32_MAX;
        }
        return (uint32_t) energy;
    }
}

/* Returns log2(value) in Q8, with value > 0. */
static int32_t fixed_log2_q8(uint32_t value) {
    uint8_t integer_part = 0;
    uint32_t temp = value;
    uint32_t scaled_q15;
    uint32_t frac_q15;

    while (temp > 1U) {
        temp >>= 1;
        integer_part++;
    }

    if (integer_part >= 15U) {
        scaled_q15 = value >> (integer_part - 15U);
    } else {
        scaled_q15 = value << (15U - integer_part);
    }

    if (scaled_q15 < 32768U) {
        scaled_q15 = 32768U;
    }
    if (scaled_q15 > 65535U) {
        scaled_q15 = 65535U;
    }

    frac_q15 = scaled_q15 - 32768U;
    return ((int32_t) integer_part * (int32_t) MODEL_QUANT_SCALE) + (int32_t) (frac_q15 >> 7);
}

void feature_extractor_compute(
    const uint8_t *samples,
    uint16_t length,
    int32_t feature_q[MODEL_FEATURE_DIM]
) {
    uint8_t c;
    uint16_t frame_count;
    int64_t mfcc_sum[MFCC_COUNT];
    uint64_t mfcc_sum_sq[MFCC_COUNT];

    for (c = 0; c < MODEL_FEATURE_DIM; c++) {
        feature_q[c] = 0;
    }

    if (samples == NULL || length == 0U) {
        return;
    }

    for (c = 0; c < MFCC_COUNT; c++) {
        mfcc_sum[c] = 0;
        mfcc_sum_sq[c] = 0;
    }

    if (length <= FRAME_LENGTH) {
        frame_count = 1U;
    } else {
        uint32_t extra = (uint32_t) length - FRAME_LENGTH;
        frame_count = (uint16_t) ((extra + HOP_LENGTH - 1U) / HOP_LENGTH + 1U);
    }

    {
        uint16_t frame_index;
        for (frame_index = 0; frame_index < frame_count; frame_index++) {
            uint16_t frame_start = (uint16_t) (frame_index * HOP_LENGTH);
            int32_t log_bands_q8[MEL_BAND_COUNT];
            uint8_t band;

            for (band = 0; band < MEL_BAND_COUNT; band++) {
                int16_t coeff_q14 = (int16_t) pgm_read_word(&k_goertzel_coeff_q14[band]);
                uint32_t power = goertzel_power(samples, length, frame_start, coeff_q14);
                log_bands_q8[band] = fixed_log2_q8(power);
            }

            for (c = 0; c < MFCC_COUNT; c++) {
                int64_t acc = 0;
                for (band = 0; band < MEL_BAND_COUNT; band++) {
                    int16_t dct_q14 = (int16_t) pgm_read_word(&k_dct_q14[c][band]);
                    acc += (int64_t) dct_q14 * log_bands_q8[band];
                }

                {
                    int32_t mfcc_q8 = (int32_t) (acc >> DCT_Q_SHIFT);
                    mfcc_sum[c] += mfcc_q8;
                    mfcc_sum_sq[c] += (uint64_t) ((int64_t) mfcc_q8 * mfcc_q8);
                }
            }
        }
    }

    for (c = 0; c < MFCC_COUNT; c++) {
        int32_t mean_q8 = (int32_t) (mfcc_sum[c] / frame_count);
        int64_t mean_sq_q16 = (int64_t) mean_q8 * mean_q8;
        int64_t var_q16 = (int64_t) (mfcc_sum_sq[c] / frame_count) - mean_sq_q16;
        uint32_t std_q8;

        if (var_q16 < 0) {
            var_q16 = 0;
        }

        std_q8 = integer_sqrt_u64((uint64_t) var_q16);

        feature_q[c] = mean_q8;
        feature_q[c + MFCC_COUNT] = (int32_t) std_q8;
    }
}
