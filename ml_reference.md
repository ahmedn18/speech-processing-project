# ML and DSP Reference for Project 1

These references are selected to support the speech-recognition system described in `project_1.pdf`.

## Recommended Reading Order

1. [MIT OCW: Lecture 21, Sampling](https://ocw.mit.edu/courses/6-003-signals-and-systems-fall-2011/resources/lecture-21-sampling/)  
   Read first for sampling, aliasing, and why the microphone front-end needs a low-pass anti-alias filter before the ADC.


###########
2. [Microchip ATmega32A Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmega32A-DataSheet-Complete-DS40002072A.pdf)  
   Focus on memory limits, timers, ADC, interrupts, and sleep modes. This is the hardware reality check for every architecture decision.

3. [Microchip: AVR ADC Operating Modes](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/8-bit-avr/structure/adc-operating-modes/)  
   Good for understanding single conversion, free-running mode, and auto-triggering from a timer.

4. [Microchip AVR126 / AN2538: ADC of megaAVR in Single-Ended Mode](https://ww1.microchip.com/downloads/en/Appnotes/AN2538-ADC-of-megaAVR-in-SingleEnded-Mode-00002538A.pdf)  
   Practical reference for timer-driven sampling on AVR.
###########


5. [Stanford EE269: Short-Time Fourier Transform](https://web.stanford.edu/class/ee269/Lecture3_part3_STFT.pdf)  
   Explains framing, windowing, hop size, and the time-frequency tradeoff. Even if FFT is not used on the AVR, this helps make frame-based processing intuitive.

6. [MIT OCW: Speech Sounds of American English](https://ocw.mit.edu/courses/6-345-automatic-speech-recognition-spring-2003/50d1a4af0b8168977cd205dba8c4f414_lecture34new.pdf)  
   Useful for intuition about why features like energy and zero-crossing rate can separate different speech sounds.

7. [Stanford SLP3, Chapter 14: Speech Recognition and Feature Extraction Background](https://web.stanford.edu/~jurafsky/slp3/14.pdf)  
   Broad background on speech signals and feature extraction.

8. [Stanford EE269: Cepstrum and MFCC](https://web.stanford.edu/class/ee269/Lecture_5_Cepstrum_MFCC.pdf)  
   Read this to understand MFCCs well enough to decide whether they should stay offline only or be simplified for the AVR.

9. [Stanford EE269: Distance-Based Signal Classification / Nearest Neighbor](https://web.stanford.edu/class/ee269/Lecture4.pdf)  
   Clean introduction to Euclidean-distance template matching, which is the simplest classifier in the project brief.

10. [MIT OCW: Pattern Classification I](https://ocw.mit.edu/courses/6-345-automatic-speech-recognition-spring-2003/1fc5500cf738fc75e77487321b27ec35_lecture7.pdf)  
    Theory behind decision rules, template matching, and feature-vector classification.

11. [MIT OCW: Dynamic Time Warping and Search](https://ocw.mit.edu/courses/6-345-automatic-speech-recognition-spring-2003/27f6a52bf2a27718ba669c86e0f3472e_lecture9.pdf)  
    Most directly relevant source for a classic isolated-word recognizer.

12. [Sakoe and Chiba (1978), "Dynamic Programming Algorithm Optimization for Spoken Word Recognition"](https://doi.org/10.1109/TASSP.1978.1163055)  
    Canonical DTW paper. Read this after the MIT DTW lecture, not before.

## Shortest Useful Path

If time is limited, read these first:

- Sampling and anti-aliasing: MIT OCW Lecture 21
- Hardware constraints: ATmega32A datasheet
- ADC on AVR: AN2538
- Frame-based processing: Stanford STFT notes
- Simple classifier: Stanford distance-based classification
- Time-aligned matching: MIT DTW lecture

## Why These Matter for the Project

- `ADC + timer`: needed for consistent `8 kHz` sampling on ATmega32A
- `framing + windows`: needed for short-time speech features
- `energy + ZCR + spectral features`: candidate lightweight features for embedded extraction
- `Euclidean distance`: simplest template-matching baseline
- `DTW`: better handling of speaking-rate variation for isolated words
- `ATmega32A datasheet`: keeps the design realistic under `2 KB` RAM and `32 KB` flash
