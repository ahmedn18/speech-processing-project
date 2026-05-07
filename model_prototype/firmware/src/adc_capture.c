#include "adc_capture.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "config.h"

static volatile uint8_t g_capture_buffer[UTTERANCE_MAX_SAMPLES];
static volatile uint16_t g_capture_length = 0;
static volatile uint8_t g_capture_ready = 0;
static volatile uint8_t g_capture_enabled = 0;
static volatile uint8_t g_capture_active = 0;
static volatile uint16_t g_start_count = 0;
static volatile uint16_t g_silence_count = 0;

static uint8_t abs_centered_amplitude(uint8_t sample) {
    return (sample >= 128U) ? (sample - 128U) : (128U - sample);
}

static void complete_capture(void) {
    g_capture_enabled = 0;
    g_capture_active = 0;
    g_capture_ready = 1;
}

ISR(ADC_vect) {
    if (!g_capture_enabled || g_capture_ready) {
        return;
    }

    uint8_t sample = ADCH;
    uint8_t amplitude = abs_centered_amplitude(sample);

    if (!g_capture_active) {
        if (amplitude >= SPEECH_START_THRESHOLD) {
            if (g_start_count < 0xFFFFU) {
                g_start_count++;
            }
            if (g_start_count >= SPEECH_START_CONSECUTIVE_SAMPLES) {
                g_capture_active = 1;
                g_capture_length = 0;
                g_silence_count = 0;
            }
        } else {
            g_start_count = 0;
        }
    }

    if (!g_capture_active) {
        return;
    }

    if (g_capture_length < UTTERANCE_MAX_SAMPLES) {
        g_capture_buffer[g_capture_length++] = sample;
    } else {
        complete_capture();
        return;
    }

    if (amplitude <= SPEECH_STOP_THRESHOLD) {
        if (g_silence_count < 0xFFFFU) {
            g_silence_count++;
        }
    } else {
        g_silence_count = 0;
    }

    if (g_capture_length >= MIN_UTTERANCE_SAMPLES && g_silence_count >= SPEECH_STOP_SILENCE_SAMPLES) {
        complete_capture();
    }
}

void adc_capture_init(void) {
    DDRA &= (uint8_t) ~(1U << PA0);

    TCCR1A = 0;
    TCCR1B = (1U << WGM12) | (1U << CS11);
    TCNT1 = 0;
    OCR1A = (uint16_t) ADC_TIMER_OCR;
    OCR1B = (uint16_t) ADC_TIMER_OCR;

    ADMUX = (1U << REFS0) | (1U << ADLAR) | (ADC_INPUT_CHANNEL & 0x07U);
    ADCSRA = (1U << ADEN) | (1U << ADATE) | (1U << ADIE) | (1U << ADPS2) | (1U << ADPS1) | (1U << ADPS0);

    SFIOR &= (uint8_t) ~((1U << ADTS2) | (1U << ADTS1) | (1U << ADTS0));
    SFIOR |= (1U << ADTS2) | (1U << ADTS0);

    ADCSRA |= (1U << ADSC);
}

void adc_capture_start(void) {
    cli();
    g_capture_enabled = 1;
    g_capture_active = 0;
    g_capture_ready = 0;
    g_capture_length = 0;
    g_start_count = 0;
    g_silence_count = 0;
    sei();
}

void adc_capture_stop(void) {
    cli();
    g_capture_enabled = 0;
    g_capture_active = 0;
    sei();
}

uint8_t adc_capture_is_ready(void) {
    return g_capture_ready;
}

void adc_capture_clear_ready(void) {
    cli();
    g_capture_ready = 0;
    sei();
}

const uint8_t *adc_capture_get_utterance(void) {
    return (const uint8_t *) g_capture_buffer;
}

uint16_t adc_capture_get_length(void) {
    uint16_t length;
    cli();
    length = g_capture_length;
    sei();
    return length;
}
