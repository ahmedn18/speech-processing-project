# Introduction

The first step in speech processing is often to transform the input waveform into a sequence of acoustic feature vectors, each vector representing the information in a small time window of
the signal.



# Steps


## 1 Sampling and quantization 

First of all, as we know the input to the speech recognizer is a complex series of changes in air pressure. Therefore, we need to to ADC that has two steps obviously: sampling and quantization which can be done easily on ATMEGA32

## 2 Windowing

Since the statistical properties for a full speech signal is not constant, hence non-stationary, we will extract information from windows of the signal, which are going to be roughly stationary proportions of speech. We will call the speech extracted from each window a **frame** which is charactarized by: window/frame size, frame stride, and the shape of the window.

Rectangular windows are great to use as a window shape and it makes sense, but it has a problem with it's boundaries being cutoff which causes problems during fourier anaylsis. So instead, we use something called **Hamming** code, which shrinks the values of the signal toward zero at the window boundaries, avoiding these discontinuities. Equations for these can be found in [./ml_reference.md] equations 14.12 and 14.13.


## 3. Discrete Fourier Transform (DFT) 


In order to extract energy information for different frequency bands within each window, we need to use DFT. Equation is in [./ml_reference.md] equation 14.15.

In order to compute this efficiently, we'll use the FFT which works for values of N that are not only powers of 2. In addition to that, it can compute the FT in O(n log n) instead of O(n ^ 2).

## 4. Mel Filter Bank and Log


Since human hearing is more sensitive to lower frequencies which is crucial for distinguishing vowels and nasals, while information in high frequencies (like stop bursts or fricative noise) is less crucial for successfull recognition. So, modelling this human perceptual property improves speech recognition performance in the same way.

We implement this intuition by creating a bank of filters that collect energy from
each frequency band, spread logarithmically so that we have very fine resolution
at low frequencies, and less resolution at high frequencies. Figure 14.29 shows a
sample bank of triangular filters that implement this idea, that can be multiplied by
the spectrum to get a mel spectrum.

Finally, we take the log of each of the mel spectrum values. The human response
to signal level is logarithmic (like the human response to frequency). Humans are
less sensitive to slight differences in amplitude at high amplitudes than at low amplitudes. In addition, using a log makes the feature estimates less sensitive to variations in input such as power variations due to the speaker’s mouth moving closer or further from the microphone.


## 5. Normalization

Before we send this log mel channel vector to the downstream neural network
layers, it’s common for speech systems to rescale them so they have comparable
ranges. A common type of normalization for speech is to scale the input to be between -1 and 1 with zero mean across the entire pretraining dataset (see Section ??
in Chapter 4).

We can use Cepstral Mean and Variance Normalization (CMVN)


## 6. MFCC: Mel Frequency Cepstral Coefficients 

This is one way to deconvolve the source and filter (the position of the vocal tract).
Information about the vocal tract are only in the first 12 cepstral coefficients + also we add a 13th feature which is the energy of the frame. To each of the 13 features (12 cepstral features plus energy) a delta double delta or velocity feature and a double delta or acceleration feature.
