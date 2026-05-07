from pathlib import Path

import numpy as np


REJECTION_MARGIN = 0.2


def _format_1d(values):
    return ", ".join(str(int(v)) for v in values)


def _format_2d(matrix):
    rows = []
    for row in matrix:
        rows.append("    {" + _format_1d(row) + "}")
    return ",\n".join(rows)


def _format_labels(labels):
    rows = []
    for label in labels:
        escaped = label.replace("\\", "\\\\").replace('"', '\\"')
        padded = escaped + "\\0"
        rows.append(f'    "{padded}"')
    return ",\n".join(rows)


def generate(npz_path: Path, include_path: Path, source_path: Path) -> None:
    data = np.load(npz_path, allow_pickle=True)
    labels = data["labels"]
    feature_mean_q = data["feature_mean_q"].astype(np.int32)
    feature_std_q = data["feature_std_q"].astype(np.int32)
    coef_q = data["coef_q"].astype(np.int32)
    intercept_q = data["intercept_q"].astype(np.int32)
    quant_scale = int(data["quant_scale"])

    feature_dim = int(feature_mean_q.shape[0])
    class_count = int(labels.shape[0])
    label_len = max(len(str(label)) for label in labels) + 1
    rejection_margin_q = int(round(REJECTION_MARGIN * quant_scale * quant_scale))

    include_path.write_text(
        (
            "#ifndef MODEL_PARAMS_H\n"
            "#define MODEL_PARAMS_H\n\n"
            "#include <stdint.h>\n"
            "#include <avr/pgmspace.h>\n\n"
            f"#define MODEL_FEATURE_DIM {feature_dim}U\n"
            f"#define MODEL_CLASS_COUNT {class_count}U\n"
            f"#define MODEL_LABEL_MAX_LEN {label_len}U\n"
            f"#define MODEL_QUANT_SCALE {quant_scale}L\n"
            f"#define MODEL_REJECTION_MARGIN_Q {rejection_margin_q}L\n\n"
            "extern const int32_t model_feature_mean_q[MODEL_FEATURE_DIM] PROGMEM;\n"
            "extern const int32_t model_feature_std_q[MODEL_FEATURE_DIM] PROGMEM;\n"
            "extern const int32_t model_coef_q[MODEL_CLASS_COUNT][MODEL_FEATURE_DIM] PROGMEM;\n"
            "extern const int32_t model_intercept_q[MODEL_CLASS_COUNT] PROGMEM;\n"
            "extern const char model_labels[MODEL_CLASS_COUNT][MODEL_LABEL_MAX_LEN] PROGMEM;\n\n"
            "#endif\n"
        ),
        encoding="ascii",
    )

    source_path.write_text(
        (
            '#include "model_params.h"\n\n'
            f"const int32_t model_feature_mean_q[MODEL_FEATURE_DIM] PROGMEM = {{{_format_1d(feature_mean_q)}}};\n\n"
            f"const int32_t model_feature_std_q[MODEL_FEATURE_DIM] PROGMEM = {{{_format_1d(feature_std_q)}}};\n\n"
            "const int32_t model_coef_q[MODEL_CLASS_COUNT][MODEL_FEATURE_DIM] PROGMEM = {\n"
            f"{_format_2d(coef_q)}\n"
            "};\n\n"
            f"const int32_t model_intercept_q[MODEL_CLASS_COUNT] PROGMEM = {{{_format_1d(intercept_q)}}};\n\n"
            "const char model_labels[MODEL_CLASS_COUNT][MODEL_LABEL_MAX_LEN] PROGMEM = {\n"
            f"{_format_labels(labels)}\n"
            "};\n"
        ),
        encoding="ascii",
    )


def main() -> None:
    firmware_root = Path(__file__).resolve().parents[1]
    project_root = firmware_root.parent

    npz_path = project_root / "linear_model.npz"
    include_path = firmware_root / "include" / "model_params.h"
    source_path = firmware_root / "src" / "model_params.c"

    generate(npz_path, include_path, source_path)
    print(f"Generated {include_path}")
    print(f"Generated {source_path}")


if __name__ == "__main__":
    main()
