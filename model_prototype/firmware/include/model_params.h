#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>
#include <avr/pgmspace.h>

#define MODEL_FEATURE_DIM 26U
#define MODEL_CLASS_COUNT 10U
#define MODEL_LABEL_MAX_LEN 6U
#define MODEL_QUANT_SCALE 256L
#define MODEL_REJECTION_MARGIN_Q 13107L

extern const int32_t model_feature_mean_q[MODEL_FEATURE_DIM] PROGMEM;
extern const int32_t model_feature_std_q[MODEL_FEATURE_DIM] PROGMEM;
extern const int32_t model_coef_q[MODEL_CLASS_COUNT][MODEL_FEATURE_DIM] PROGMEM;
extern const int32_t model_intercept_q[MODEL_CLASS_COUNT] PROGMEM;
extern const char model_labels[MODEL_CLASS_COUNT][MODEL_LABEL_MAX_LEN] PROGMEM;

#endif
