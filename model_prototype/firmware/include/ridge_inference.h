#ifndef RIDGE_INFERENCE_H
#define RIDGE_INFERENCE_H

#include <stdint.h>

#include "model_params.h"

void ridge_model_init(void);
uint8_t ridge_model_uses_external(void);

int8_t ridge_predict(
    const int32_t raw_features_q[MODEL_FEATURE_DIM],
    int32_t rejection_margin_q,
    int32_t *margin_out_q
);

void ridge_copy_label(uint8_t label_index, char *dest, uint8_t dest_len);

#endif
