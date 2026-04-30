"""Train/val/test split and best ridge classifier."""

from pathlib import Path

import librosa
import numpy as np
from sklearn.linear_model import RidgeClassifier


ROOT = Path(__file__).resolve().parents[1] / "recordings"
ALL_FEATURES_CSV = Path(__file__).with_name("labeled_features.csv")
TRAIN_FEATURES_CSV = Path(__file__).with_name("train_features.csv")
VAL_FEATURES_CSV = Path(__file__).with_name("val_features.csv")
TEST_FEATURES_CSV = Path(__file__).with_name("test_features.csv")
MODEL_FILE = Path(__file__).with_name("linear_model.npz")

SAMPLE_RATE = 22050
FRAME_LENGTH = 256
HOP_LENGTH = 128
N_MELS = 20
N_MFCC = 13
RNG_SEED = 14
ALPHAS = (0.1, 1.0, 10.0, 100.0)
REJECTION_MARGIN = 0.2
QUANT_SCALE = 256


def load_wav_paths(directory):
    return sorted(Path(directory).glob("*.wav"))


def discover_samples():
    samples = []
    for path in load_wav_paths(ROOT / "wav_files"):
        samples.append((path, path.stem))
    for path in load_wav_paths(ROOT / "test_data"):
        samples.append((path, path.stem))
    for directory in sorted(ROOT.glob("*-recordings")):
        label = directory.name.removesuffix("-recordings")
        for path in directory.rglob("*.wav"):
            samples.append((path, label))
    return samples


def normalize_waveform(signal):
    signal = np.asarray(signal, dtype=np.float32)
    signal = signal - np.mean(signal)
    peak = np.max(np.abs(signal))
    if peak > 0:
        signal = signal / peak
    return signal


def align_waveform(signal):
    trimmed, _ = librosa.effects.trim(signal, top_db=25)
    return normalize_waveform(trimmed if len(trimmed) else signal)


def ensure_min_frames(matrix, min_frames=3):
    if matrix.shape[1] >= min_frames:
        return matrix
    if matrix.shape[1] == 0:
        return np.zeros((matrix.shape[0], min_frames), dtype=matrix.dtype)
    return np.pad(matrix, ((0, 0), (0, min_frames - matrix.shape[1])), mode="edge")


def extract_features(signal, sr):
    mfcc = librosa.feature.mfcc(
        y=signal,
        sr=sr,
        n_mfcc=N_MFCC,
        n_mels=N_MELS,
        n_fft=FRAME_LENGTH,
        hop_length=HOP_LENGTH,
    )
    mfcc = ensure_min_frames(mfcc)
    return np.concatenate([mfcc.mean(axis=1), mfcc.std(axis=1)])


def build_rows(samples, label_to_id):
    rows = []
    for path, label in samples:
        signal, sr = librosa.load(path, sr=SAMPLE_RATE)
        signal = align_waveform(signal)
        feature_row = extract_features(signal, sr)
        rows.append(np.concatenate([feature_row, [label_to_id[label]]]))
    if not rows:
        raise ValueError("No feature vectors were extracted from the available files")
    return np.vstack(rows)


def stratified_split(samples, rng):
    grouped = {}
    for path, label in samples:
        grouped.setdefault(label, []).append((path, label))

    train = []
    val = []
    test = []
    for label in sorted(grouped):
        rows = grouped[label].copy()
        rng.shuffle(rows)
        n = len(rows)
        n_train = max(1, int(round(n * 0.7)))
        n_val = max(1, int(round(n * 0.15))) if n >= 3 else 0
        if n_train + n_val >= n:
            n_train = max(1, n - 2)
            n_val = 1 if n - n_train > 1 else 0
        train.extend(rows[:n_train])
        val.extend(rows[n_train : n_train + n_val])
        test.extend(rows[n_train + n_val :])
    return train, val, test


def write_split_csv(path, data):
    fmt = ["%.6f"] * (data.shape[1] - 1) + ["%d"]
    np.savetxt(path, data, delimiter=",", fmt=fmt)


def scale_features(features, feature_mean, feature_std):
    return (features - feature_mean) / feature_std


def fit_ridge_classifier(features, labels, alpha):
    feature_mean = features.mean(axis=0)
    feature_std = features.std(axis=0)
    feature_std[feature_std == 0] = 1.0
    scaled = scale_features(features, feature_mean, feature_std)

    classifier = RidgeClassifier(alpha=alpha)
    classifier.fit(scaled, labels)
    return feature_mean, feature_std, classifier


def score_samples(features, feature_mean, feature_std, classifier):
    scaled = scale_features(features, feature_mean, feature_std)
    return classifier.decision_function(scaled)


def predict_samples(features, feature_mean, feature_std, classifier):
    scores = score_samples(features, feature_mean, feature_std, classifier)
    if scores.ndim == 1:
        return (scores > 0).astype(int)
    return np.argmax(scores, axis=1)


def accuracy(features, labels, feature_mean, feature_std, classifier):
    predicted = predict_samples(features, feature_mean, feature_std, classifier)
    return float(np.mean(predicted == labels))


def confidence_margin(scores):
    sorted_scores = np.sort(scores, axis=1)
    return sorted_scores[:, -1] - sorted_scores[:, -2]


def quantized_accuracy(features, labels, feature_mean, feature_std, classifier, scale):
    standardized = np.round(scale_features(features, feature_mean, feature_std) * scale).astype(np.int64)
    coef_q = np.round(classifier.coef_ * scale).astype(np.int64)
    intercept_q = np.round(np.asarray(classifier.intercept_) * scale * scale).astype(np.int64)
    scores = standardized @ coef_q.T + intercept_q
    if scores.ndim == 1:
        predicted = (scores > 0).astype(int)
    else:
        predicted = np.argmax(scores, axis=1)
    return float(np.mean(predicted == labels))


def select_alpha(train_x, train_y, val_x, val_y):
    best = None
    for alpha in ALPHAS:
        feature_mean, feature_std, classifier = fit_ridge_classifier(train_x, train_y, alpha)
        val_accuracy = accuracy(val_x, val_y, feature_mean, feature_std, classifier)
        if (
            best is None
            or val_accuracy > best["val_accuracy"]
            or (np.isclose(val_accuracy, best["val_accuracy"]) and alpha > best["alpha"])
        ):
            best = {
                "alpha": alpha,
                "val_accuracy": val_accuracy,
                "feature_mean": feature_mean,
                "feature_std": feature_std,
                "classifier": classifier,
            }
    return best


samples = discover_samples()
if not samples:
    raise ValueError("No audio files found in ../recordings/")

label_names = sorted({label for _, label in samples})
label_to_id = {name: idx for idx, name in enumerate(label_names)}

rng = np.random.default_rng(RNG_SEED)
train_samples, val_samples, test_samples = stratified_split(samples, rng)

train_data = build_rows(train_samples, label_to_id)
val_data = build_rows(val_samples, label_to_id)
test_data = build_rows(test_samples, label_to_id)

train_x = train_data[:, :-1]
train_y = train_data[:, -1].astype(int)
val_x = val_data[:, :-1]
val_y = val_data[:, -1].astype(int)
test_x = test_data[:, :-1]
test_y = test_data[:, -1].astype(int)

best = select_alpha(train_x, train_y, val_x, val_y)

train_val_x = np.vstack([train_x, val_x])
train_val_y = np.concatenate([train_y, val_y])
feature_mean_full, feature_std_full, classifier_full = fit_ridge_classifier(train_val_x, train_val_y, best["alpha"])

val_accuracy = accuracy(val_x, val_y, best["feature_mean"], best["feature_std"], best["classifier"])
test_scores = score_samples(test_x, feature_mean_full, feature_std_full, classifier_full)
test_predictions = np.argmax(test_scores, axis=1)
test_margins = confidence_margin(test_scores)
test_accuracy = float(np.mean(test_predictions == test_y))
selective_mask = test_margins >= REJECTION_MARGIN
selective_coverage = float(np.mean(selective_mask))
selective_accuracy = (
    float(np.mean(test_predictions[selective_mask] == test_y[selective_mask])) if np.any(selective_mask) else 0.0
)
quantized_test_accuracy = quantized_accuracy(
    test_x, test_y, feature_mean_full, feature_std_full, classifier_full, QUANT_SCALE
)

feature_mean_q = np.round(feature_mean_full * QUANT_SCALE).astype(np.int32)
feature_std_q = np.round(feature_std_full * QUANT_SCALE).astype(np.int32)
coef_q = np.round(classifier_full.coef_ * QUANT_SCALE).astype(np.int32)
intercept_q = np.round(np.asarray(classifier_full.intercept_) * QUANT_SCALE * QUANT_SCALE).astype(np.int32)

all_data = np.vstack([train_data, val_data, test_data])

write_split_csv(ALL_FEATURES_CSV, all_data)
write_split_csv(TRAIN_FEATURES_CSV, train_data)
write_split_csv(VAL_FEATURES_CSV, val_data)
write_split_csv(TEST_FEATURES_CSV, test_data)
np.savez(
    MODEL_FILE,
    labels=np.array(label_names, dtype="U"),
    feature_mean=feature_mean_full,
    feature_std=feature_std_full,
    coef=classifier_full.coef_,
    intercept=classifier_full.intercept_,
    feature_mean_q=feature_mean_q,
    feature_std_q=feature_std_q,
    coef_q=coef_q,
    intercept_q=intercept_q,
    sample_rate=SAMPLE_RATE,
    frame_length=FRAME_LENGTH,
    hop_length=HOP_LENGTH,
    n_mels=N_MELS,
    n_mfcc=N_MFCC,
    alpha=best["alpha"],
    quant_scale=QUANT_SCALE,
)

print(f"Labels: {label_to_id}")
print(f"Split sizes: train={len(train_data)} val={len(val_data)} test={len(test_data)}")
print(f"Saved labeled dataset to {ALL_FEATURES_CSV}")
print(f"Saved train/val/test CSVs to {TRAIN_FEATURES_CSV}, {VAL_FEATURES_CSV}, {TEST_FEATURES_CSV}")
print(f"Sample rate: {SAMPLE_RATE}")
print(f"Chosen n_mels: {N_MELS}")
print(f"Chosen n_mfcc: {N_MFCC}")
print(f"Selected ridge alpha: {best['alpha']}")
print(f"Validation accuracy: {val_accuracy:.2%}")
print(f"Test accuracy: {test_accuracy:.2%}")
print(f"Quantized test accuracy @ scale={QUANT_SCALE}: {quantized_test_accuracy:.2%}")
print(f"Selective accuracy @ margin>={REJECTION_MARGIN}: {selective_accuracy:.2%}")
print(f"Selective coverage @ margin>={REJECTION_MARGIN}: {selective_coverage:.2%}")
print(f"Saved ridge model to {MODEL_FILE}")
