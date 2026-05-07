#include <avr/interrupt.h>
#include <util/delay.h>

#include "adc_capture.h"
#include "config.h"
#include "feature_extractor.h"
#include "lcd.h"
#include "ridge_inference.h"
#include "uart.h"

static int32_t g_features_q[MODEL_FEATURE_DIM];
static char g_label_buffer[MODEL_LABEL_MAX_LEN];

int main(void) {
    lcd_init();
    uart_init(UART_BAUDRATE);
    ridge_model_init();
    adc_capture_init();

    sei();

    lcd_clear();
    lcd_print("Speech Ready");
    lcd_set_cursor(1, 0);
    if (ridge_model_uses_external()) {
        lcd_print("Model:24C512");
        uart_write_text("Model source: 24C512 external EEPROM\r\n");
    } else {
        lcd_print("Model:Flash");
        uart_write_text("Model source: internal flash fallback\r\n");
    }
    uart_write_text("Speech firmware ready\r\n");

    _delay_ms(600);
    lcd_clear();
    lcd_print("Listening...");

    adc_capture_start();

    while (1) {
        if (!adc_capture_is_ready()) {
            continue;
        }

        const uint8_t *utterance = adc_capture_get_utterance();
        uint16_t length = adc_capture_get_length();
        adc_capture_clear_ready();

        feature_extractor_compute(utterance, length, g_features_q);

        int32_t margin_q = 0;
        int8_t predicted_label = ridge_predict(g_features_q, MODEL_REJECTION_MARGIN_Q, &margin_q);

        lcd_clear();
        if (predicted_label < 0) {
            lcd_print("Unknown");
            lcd_set_cursor(1, 0);
            lcd_print("Try again");
            uart_write_text("Prediction: unknown\r\n");
        } else {
            ridge_copy_label((uint8_t) predicted_label, g_label_buffer, MODEL_LABEL_MAX_LEN);
            lcd_print("Word:");
            lcd_set_cursor(1, 0);
            lcd_print(g_label_buffer);
            uart_write_text("Prediction: ");
            uart_write_text(g_label_buffer);
            uart_write_text("\r\n");
        }

        _delay_ms(300);
        adc_capture_start();
    }
}
