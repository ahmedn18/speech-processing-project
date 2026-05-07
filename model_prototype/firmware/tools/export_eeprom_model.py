from pathlib import Path
import struct
import zlib

import numpy as np


MAGIC = b"SRM1"
VERSION = 1
HEADER_BYTES = 32


def _le_i32_array(values: np.ndarray) -> bytes:
    return b"".join(struct.pack("<i", int(v)) for v in values.reshape(-1))


def build_payload(npz_path: Path) -> tuple[bytes, int, int]:
    data = np.load(npz_path, allow_pickle=True)

    feature_mean_q = data["feature_mean_q"].astype(np.int32)
    feature_std_q = data["feature_std_q"].astype(np.int32)
    coef_q = data["coef_q"].astype(np.int32)
    intercept_q = data["intercept_q"].astype(np.int32)
    labels = data["labels"]

    class_count = int(labels.shape[0])
    feature_dim = int(feature_mean_q.shape[0])
    label_len = max(len(str(label)) for label in labels) + 1

    label_blob = bytearray()
    for label in labels:
        encoded = str(label).encode("ascii")
        padded = encoded + b"\x00"
        if len(padded) < label_len:
            padded += b"\x00" * (label_len - len(padded))
        label_blob.extend(padded[:label_len])

    payload = b"".join(
        [
            _le_i32_array(feature_mean_q),
            _le_i32_array(feature_std_q),
            _le_i32_array(coef_q),
            _le_i32_array(intercept_q),
            bytes(label_blob),
        ]
    )
    return payload, class_count, feature_dim


def build_image(npz_path: Path, output_path: Path) -> None:
    data = np.load(npz_path, allow_pickle=True)
    quant_scale = int(data["quant_scale"])
    rejection_margin_q = int(round(0.2 * quant_scale * quant_scale))

    labels = data["labels"]
    label_len = max(len(str(label)) for label in labels) + 1

    payload, class_count, feature_dim = build_payload(npz_path)
    payload_crc32 = zlib.crc32(payload) & 0xFFFFFFFF

    header = bytearray(HEADER_BYTES)
    header[0:4] = MAGIC
    struct.pack_into("<H", header, 4, VERSION)
    struct.pack_into("<H", header, 6, feature_dim)
    struct.pack_into("<H", header, 8, class_count)
    struct.pack_into("<H", header, 10, label_len)
    struct.pack_into("<i", header, 12, quant_scale)
    struct.pack_into("<i", header, 16, rejection_margin_q)
    struct.pack_into("<I", header, 20, len(payload))
    struct.pack_into("<I", header, 24, payload_crc32)

    output_path.write_bytes(bytes(header) + payload)
    print(f"Generated EEPROM image: {output_path}")
    print(f"Payload bytes: {len(payload)}")
    print(f"CRC32: 0x{payload_crc32:08X}")


def main() -> None:
    firmware_root = Path(__file__).resolve().parents[1]
    project_root = firmware_root.parent

    npz_path = project_root / "linear_model.npz"
    output_path = firmware_root / "model_24c512.bin"
    build_image(npz_path, output_path)


if __name__ == "__main__":
    main()
