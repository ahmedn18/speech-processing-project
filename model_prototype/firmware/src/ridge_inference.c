#include "ridge_inference.h"

#include <avr/pgmspace.h>
#include <limits.h>
#include <stddef.h>

#include "eeprom24c512.h"

#define MODEL_EEPROM_MAGIC_0 'S'
#define MODEL_EEPROM_MAGIC_1 'R'
#define MODEL_EEPROM_MAGIC_2 'M'
#define MODEL_EEPROM_MAGIC_3 '1'

#define MODEL_EEPROM_HEADER_BYTES 32U
#define MODEL_EEPROM_PAYLOAD_BASE MODEL_EEPROM_HEADER_BYTES

#define MODEL_EEPROM_OFF_MAGIC 0U
#define MODEL_EEPROM_OFF_VERSION 4U
#define MODEL_EEPROM_OFF_FEATURE_DIM 6U
#define MODEL_EEPROM_OFF_CLASS_COUNT 8U
#define MODEL_EEPROM_OFF_LABEL_LEN 10U
#define MODEL_EEPROM_OFF_QUANT_SCALE 12U
#define MODEL_EEPROM_OFF_REJECTION_MARGIN 16U
#define MODEL_EEPROM_OFF_PAYLOAD_SIZE 20U
#define MODEL_EEPROM_OFF_PAYLOAD_CRC32 24U

#define MODEL_EEPROM_PAYLOAD_BYTES \
    ((MODEL_FEATURE_DIM * 4U) + (MODEL_FEATURE_DIM * 4U) + (MODEL_CLASS_COUNT * MODEL_FEATURE_DIM * 4U) + \
        (MODEL_CLASS_COUNT * 4U) + (MODEL_CLASS_COUNT * MODEL_LABEL_MAX_LEN))

#define MODEL_EEPROM_OFF_FEATURE_MEAN 0U
#define MODEL_EEPROM_OFF_FEATURE_STD (MODEL_EEPROM_OFF_FEATURE_MEAN + (MODEL_FEATURE_DIM * 4U))
#define MODEL_EEPROM_OFF_COEF (MODEL_EEPROM_OFF_FEATURE_STD + (MODEL_FEATURE_DIM * 4U))
#define MODEL_EEPROM_OFF_INTERCEPT (MODEL_EEPROM_OFF_COEF + (MODEL_CLASS_COUNT * MODEL_FEATURE_DIM * 4U))
#define MODEL_EEPROM_OFF_LABELS (MODEL_EEPROM_OFF_INTERCEPT + (MODEL_CLASS_COUNT * 4U))

static uint8_t g_use_external_model = 0U;

static int32_t read_le_i32(const uint8_t bytes[4]) {
    return (int32_t) (
        ((uint32_t) bytes[0]) | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24)
    );
}

static uint16_t read_le_u16(const uint8_t *bytes) {
    return (uint16_t) (((uint16_t) bytes[0]) | ((uint16_t) bytes[1] << 8));
}

static uint32_t read_le_u32(const uint8_t *bytes) {
    return ((uint32_t) bytes[0]) | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24);
}

static uint32_t crc32_update(uint32_t crc, uint8_t data) {
    uint8_t i;
    crc ^= data;
    for (i = 0; i < 8U; i++) {
        if (crc & 1U) {
            crc = (crc >> 1) ^ 0xEDB88320UL;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

static uint8_t external_read_i32(uint16_t payload_offset, int32_t *out_value) {
    uint8_t bytes[4];
    if (!eeprom24c512_read((uint16_t) (MODEL_EEPROM_PAYLOAD_BASE + payload_offset), bytes, 4U)) {
        return 0U;
    }
    *out_value = read_le_i32(bytes);
    return 1U;
}

static int32_t read_feature_mean(uint8_t index) {
    if (!g_use_external_model) {
        return (int32_t) pgm_read_dword(&model_feature_mean_q[index]);
    }

    {
        int32_t value = 0;
        if (!external_read_i32((uint16_t) (MODEL_EEPROM_OFF_FEATURE_MEAN + index * 4U), &value)) {
            g_use_external_model = 0U;
            return (int32_t) pgm_read_dword(&model_feature_mean_q[index]);
        }
        return value;
    }
}

static int32_t read_feature_std(uint8_t index) {
    if (!g_use_external_model) {
        return (int32_t) pgm_read_dword(&model_feature_std_q[index]);
    }

    {
        int32_t value = 0;
        if (!external_read_i32((uint16_t) (MODEL_EEPROM_OFF_FEATURE_STD + index * 4U), &value)) {
            g_use_external_model = 0U;
            return (int32_t) pgm_read_dword(&model_feature_std_q[index]);
        }
        return value;
    }
}

static int32_t read_coef(uint8_t class_index, uint8_t feature_index) {
    if (!g_use_external_model) {
        return (int32_t) pgm_read_dword(&model_coef_q[class_index][feature_index]);
    }

    {
        int32_t value = 0;
        uint16_t payload_offset = (uint16_t) (
            MODEL_EEPROM_OFF_COEF + ((uint16_t) class_index * MODEL_FEATURE_DIM + feature_index) * 4U
        );
        if (!external_read_i32(payload_offset, &value)) {
            g_use_external_model = 0U;
            return (int32_t) pgm_read_dword(&model_coef_q[class_index][feature_index]);
        }
        return value;
    }
}

static int32_t read_intercept(uint8_t class_index) {
    if (!g_use_external_model) {
        return (int32_t) pgm_read_dword(&model_intercept_q[class_index]);
    }

    {
        int32_t value = 0;
        if (!external_read_i32((uint16_t) (MODEL_EEPROM_OFF_INTERCEPT + class_index * 4U), &value)) {
            g_use_external_model = 0U;
            return (int32_t) pgm_read_dword(&model_intercept_q[class_index]);
        }
        return value;
    }
}

static int32_t standardize_feature_q(int32_t raw_q, uint8_t feature_index) {
    int32_t mean_q = read_feature_mean(feature_index);
    int32_t std_q = read_feature_std(feature_index);
    if (std_q == 0) {
        std_q = 1;
    }
    return ((raw_q - mean_q) * MODEL_QUANT_SCALE) / std_q;
}

void ridge_model_init(void) {
    uint8_t header[MODEL_EEPROM_HEADER_BYTES];
    uint8_t block[16];
    uint16_t remaining;
    uint16_t address;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t expected_crc;
    uint32_t payload_size;

    g_use_external_model = 0U;
    eeprom24c512_init();

    if (!eeprom24c512_probe()) {
        return;
    }

    if (!eeprom24c512_read(0U, header, MODEL_EEPROM_HEADER_BYTES)) {
        return;
    }

    if (
        header[MODEL_EEPROM_OFF_MAGIC + 0U] != MODEL_EEPROM_MAGIC_0 ||
        header[MODEL_EEPROM_OFF_MAGIC + 1U] != MODEL_EEPROM_MAGIC_1 ||
        header[MODEL_EEPROM_OFF_MAGIC + 2U] != MODEL_EEPROM_MAGIC_2 ||
        header[MODEL_EEPROM_OFF_MAGIC + 3U] != MODEL_EEPROM_MAGIC_3
    ) {
        return;
    }

    if (read_le_u16(&header[MODEL_EEPROM_OFF_VERSION]) != 1U) {
        return;
    }
    if (read_le_u16(&header[MODEL_EEPROM_OFF_FEATURE_DIM]) != MODEL_FEATURE_DIM) {
        return;
    }
    if (read_le_u16(&header[MODEL_EEPROM_OFF_CLASS_COUNT]) != MODEL_CLASS_COUNT) {
        return;
    }
    if (read_le_u16(&header[MODEL_EEPROM_OFF_LABEL_LEN]) != MODEL_LABEL_MAX_LEN) {
        return;
    }
    if (read_le_i32(&header[MODEL_EEPROM_OFF_QUANT_SCALE]) != MODEL_QUANT_SCALE) {
        return;
    }
    if (read_le_i32(&header[MODEL_EEPROM_OFF_REJECTION_MARGIN]) != MODEL_REJECTION_MARGIN_Q) {
        return;
    }

    payload_size = read_le_u32(&header[MODEL_EEPROM_OFF_PAYLOAD_SIZE]);
    if (payload_size != MODEL_EEPROM_PAYLOAD_BYTES) {
        return;
    }

    expected_crc = read_le_u32(&header[MODEL_EEPROM_OFF_PAYLOAD_CRC32]);
    remaining = (uint16_t) payload_size;
    address = MODEL_EEPROM_PAYLOAD_BASE;

    while (remaining > 0U) {
        uint8_t chunk = (remaining > sizeof(block)) ? (uint8_t) sizeof(block) : (uint8_t) remaining;
        uint8_t i;

        if (!eeprom24c512_read(address, block, chunk)) {
            return;
        }

        for (i = 0; i < chunk; i++) {
            crc = crc32_update(crc, block[i]);
        }

        address = (uint16_t) (address + chunk);
        remaining = (uint16_t) (remaining - chunk);
    }

    crc ^= 0xFFFFFFFFUL;
    if (crc != expected_crc) {
        return;
    }

    g_use_external_model = 1U;
}

uint8_t ridge_model_uses_external(void) {
    return g_use_external_model;
}

int8_t ridge_predict(
    const int32_t raw_features_q[MODEL_FEATURE_DIM],
    int32_t rejection_margin_q,
    int32_t *margin_out_q
) {
    uint8_t class_index;
    uint8_t feature_index;
    int32_t best_score = INT32_MIN;
    int32_t second_best_score = INT32_MIN;
    int8_t best_label = -1;

    for (class_index = 0; class_index < MODEL_CLASS_COUNT; class_index++) {
        int64_t score = read_intercept(class_index);

        for (feature_index = 0; feature_index < MODEL_FEATURE_DIM; feature_index++) {
            int32_t standardized_q = standardize_feature_q(raw_features_q[feature_index], feature_index);
            int32_t coef_q = read_coef(class_index, feature_index);
            score += (int64_t) standardized_q * coef_q;
        }

        if (score > best_score) {
            second_best_score = best_score;
            best_score = (int32_t) score;
            best_label = (int8_t) class_index;
        } else if (score > second_best_score) {
            second_best_score = (int32_t) score;
        }
    }

    if (margin_out_q != NULL) {
        *margin_out_q = best_score - second_best_score;
    }

    if ((best_score - second_best_score) < rejection_margin_q) {
        return -1;
    }
    return best_label;
}

void ridge_copy_label(uint8_t label_index, char *dest, uint8_t dest_len) {
    uint8_t i;

    if (dest == NULL || dest_len == 0U) {
        return;
    }

    if (label_index >= MODEL_CLASS_COUNT) {
        dest[0] = '?';
        if (dest_len > 1U) {
            dest[1] = '\0';
        }
        return;
    }

    if (!g_use_external_model) {
        for (i = 0; i < (uint8_t) (dest_len - 1U); i++) {
            char c = (char) pgm_read_byte(&model_labels[label_index][i]);
            dest[i] = c;
            if (c == '\0') {
                return;
            }
        }
        dest[dest_len - 1U] = '\0';
        return;
    }

    for (i = 0; i < (uint8_t) (dest_len - 1U); i++) {
        uint8_t c = 0;
        uint16_t payload_offset =
            (uint16_t) (MODEL_EEPROM_OFF_LABELS + ((uint16_t) label_index * MODEL_LABEL_MAX_LEN) + i);

        if (!eeprom24c512_read((uint16_t) (MODEL_EEPROM_PAYLOAD_BASE + payload_offset), &c, 1U)) {
            g_use_external_model = 0U;
            ridge_copy_label(label_index, dest, dest_len);
            return;
        }

        dest[i] = (char) c;
        if (c == '\0') {
            return;
        }
    }

    dest[dest_len - 1U] = '\0';
}
