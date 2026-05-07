#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include <stdint.h>

#include "model_params.h"

void feature_extractor_compute(
    const uint8_t *samples,
    uint16_t length,
    int32_t feature_q[MODEL_FEATURE_DIM]
);

#endif
