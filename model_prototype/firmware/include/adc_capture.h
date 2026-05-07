#ifndef ADC_CAPTURE_H
#define ADC_CAPTURE_H

#include <stdint.h>

void adc_capture_init(void);
void adc_capture_start(void);
void adc_capture_stop(void);
uint8_t adc_capture_is_ready(void);
void adc_capture_clear_ready(void);
const uint8_t *adc_capture_get_utterance(void);
uint16_t adc_capture_get_length(void);

#endif
