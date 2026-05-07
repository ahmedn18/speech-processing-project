# Current Best Model Architecture

## Overview
The best-performing model in `prototype.py` is a compact **ridge-classifier pipeline** built for isolated-word speech recognition. It keeps the model simple enough for deployment while preserving the strongest test performance found so far.

## Data flow
1. Discover all `.wav` files under:
   - `../recordings/wav_files/`
   - `../recordings/test_data/`
   - `../recordings/*-recordings/`
2. Assign labels from the filename stem or folder name.
3. Split the data with a fixed random seed into:
   - 70% train
   - 15% validation
   - 15% test
4. Train on the training split, select the best regularization strength on validation, then retrain on train + validation.

## Audio preprocessing
- **Sample rate:** `22050 Hz`
- **Trim silence:** `librosa.effects.trim(..., top_db=25)`
- **Normalize:** zero-mean, peak-normalized waveform

This keeps the feature extractor focused on the spoken word itself instead of leading/trailing silence.

## Feature extraction
The model uses a compact MFCC summary vector:

- **MFCC count:** `13`
- **Mel bands:** `20`
- **FFT window:** `256`
- **Hop length:** `128`

For each clip, the pipeline computes:
- MFCC mean per coefficient
- MFCC standard deviation per coefficient

So the final feature vector is:

- `13 mean values`
- `13 standard deviation values`
- **Total:** `26 features`

## Classifier
The classifier is a **RidgeClassifier**.

### Hyperparameter search
The validation sweep checks:
- `alpha = 0.1`
- `alpha = 1.0`
- `alpha = 10.0`
- `alpha = 100.0`

The best model is the one with the highest validation accuracy. If two values tie, the larger `alpha` wins.

### Final training
After selection, the model is retrained on:
- training split
- validation split

That final model is the one saved to disk and used for test scoring.

## Decision logic
The classifier uses the ridge model scores directly:
- `decision_function()` gives class scores
- the predicted label is the class with the highest score

For binary cases, the score sign is used.

## Rejection / confidence
The script also computes a simple confidence margin:
- sort class scores
- subtract second-best from best

If the margin is at least `0.2`, the prediction is treated as confident.

## Quantized export
To keep deployment simple, the model also stores a fixed-point version:
- `feature_mean_q`
- `feature_std_q`
- `coef_q`
- `intercept_q`

Quantization scale:
- `256`

## Saved artifact
The model is stored in `linear_model.npz` with:
- label list
- feature mean/std
- ridge coefficients
- ridge intercept
- quantized equivalents
- sample rate and feature settings
- selected `alpha`

## Current performance
Latest run:
- **Validation accuracy:** `52.38%`
- **Test accuracy:** `78.26%`
- **Quantized test accuracy:** `69.57%`

## Why this is the best current model
It is the best balance found so far between:
- accuracy
- simplicity
- low feature size
- easy deployment

It outperformed the alternative compact variants tested earlier while staying small enough to port later.
