#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "adc_capture.h"
#include "config.h"
#include "feature_extractor.h"
#include "lcd.h"
#include "ridge_inference.h"
#include "uart.h"

#if defined(MCUSR)
#define RESET_STATUS_REG MCUSR
#elif defined(MCUCSR)
#define RESET_STATUS_REG MCUCSR
#else
#error "Unsupported AVR reset status register"
#endif

static int32_t g_features_q[MODEL_FEATURE_DIM];
static char g_label_buffer[MODEL_LABEL_MAX_LEN];
static uint8_t g_reset_flags __attribute__((section(".noinit")));

void capture_reset_flags(void) __attribute__((naked, used, section(".init3")));

void capture_reset_flags(void) {
    g_reset_flags = RESET_STATUS_REG;
    RESET_STATUS_REG = 0U;
    wdt_disable();
}

static void uart_write_reset_flags(uint8_t flags) {
    uart_write_text("Reset cause:");
    if (flags == 0U) {
        uart_write_text(" none");
    }
    if (flags & (1U << PORF)) {
        uart_write_text(" POR");
    }
    if (flags & (1U << EXTRF)) {
        uart_write_text(" EXT");
    }
    if (flags & (1U << BORF)) {
        uart_write_text(" BOR");
    }
    if (flags & (1U << WDRF)) {
        uart_write_text(" WDT");
    }
    uart_write_text("\r\n");
}

int main(void) {
    uart_init(UART_BAUDRATE);
    uart_write_text("Boot: UART OK\r\n");
    uart_write_reset_flags(g_reset_flags);

    LCD_Init();
    ridge_model_init();
    adc_capture_init();

    sei();

    LCD_Clear();
    LCD_String("Speech Ready");
    LCD_Gotoxy(1, 0);
    if (ridge_model_uses_external()) {
        LCD_String("Model:24C512");
        uart_write_text("Model source: 24C512 external EEPROM\r\n");
    } else {
        LCD_String("Model:Flash");
        uart_write_text("Model source: internal flash fallback\r\n");
    }
    uart_write_text("Speech firmware ready\r\n");

    _delay_ms(600);
    LCD_Clear();
    LCD_String("Listening...");

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

        LCD_Clear();
        if (predicted_label < 0) {
            LCD_String("Unknown");
            LCD_Gotoxy(1, 0);
            LCD_String("Try again");
            uart_write_text("Prediction: unknown\r\n");
        } else {
            ridge_copy_label((uint8_t) predicted_label, g_label_buffer, MODEL_LABEL_MAX_LEN);
            LCD_String("Word:");
            LCD_Gotoxy(1, 0);
            LCD_String(g_label_buffer);
            uart_write_text("Prediction: ");
            uart_write_text(g_label_buffer);
            uart_write_text("\r\n");
        }

        _delay_ms(300);
        adc_capture_start();
    }
}
