# Firmware (ATmega32A)

This folder contains the first firmware implementation of the architecture:

`Mic -> ADC ISR capture -> feature extraction -> ridge inference -> LCD/UART output`

## Storage architecture

- **ATmega32A Flash (internal):** executable firmware + fallback model (`model_params.c`)
- **ATmega32A SRAM (internal):** runtime buffers (audio capture, features, scoring)
- **ATmega32A EEPROM (internal):** reserved for small persistent settings (not used yet)
- **24C512 (external EEPROM):** primary model blob (header + payload + CRC32)

At boot, firmware tries to load model metadata from 24C512 first. If header/CRC checks fail, it automatically falls back to the flash model.

## Modules

- `src/adc_capture.c`: timer-triggered ADC capture with speech start/stop detection
- `src/feature_extractor.c`: fixed-point MFCC-style feature extraction (log-mel via Goertzel bands + DCT, exported as 13 mean + 13 std)
- `src/ridge_inference.c`: quantized ridge scoring using exported model weights
- `src/lcd.c`: HD44780 16x2 4-bit LCD driver
- `src/uart.c`: UART TX debug output
- `src/twi.c`: AVR TWI/I2C transport
- `src/eeprom24c512.c`: 24C512 read/probe driver
- `src/main.c`: application loop

## Model export

The firmware reads parameters generated from `../linear_model.npz`.

1. Activate your Python environment with `numpy`.
2. Generate model files:

```bash
make model
```

This generates:

- `include/model_params.h`
- `src/model_params.c`

To generate the 24C512 external EEPROM image:

```bash
make eeprom-image
```

This generates:

- `model_24c512.bin`

Program that binary into 24C512 starting at address `0x0000`.

## Build

```bash
make
```

Produces:

- `firmware.elf`
- `firmware.hex`

## Flash (example)

```bash
avrdude -p m32 -c usbasp -U flash:w:firmware.hex
```

## Notes

- The current extractor is MFCC-style and fixed-point. It is designed to stay within ATmega32A RAM limits while producing the same 26-element shape expected by the ridge model.
- It is an embedded approximation (Goertzel-based log-mel bands) rather than a full librosa-equivalent MFCC pipeline.
- Model source is reported on LCD/UART at startup as either `24C512` or `Flash`.
