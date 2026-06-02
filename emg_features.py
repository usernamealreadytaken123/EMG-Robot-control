from __future__ import annotations

from typing import Iterable

import numpy as np


SUPPORTED_GESTURES = ("relax", "fist", "open", "left", "right")

GESTURE_COMMANDS = {
    "relax": "S",
    "fist": "F",
    "open": "B",
    "left": "L",
    "right": "R",
}

EMERGENCY_COMMAND = "S"

FEATURE_COLUMNS = [
    "rms",
    "mav",
    "zc",
    "wl",
    "std",
    "c_rms",
    "c_mav",
    "c_zc",
    "c_wl",
    "c_std",
    "mean",
    "median",
    "min",
    "max",
    "ptp",
    "q25",
    "q75",
    "iemg",
]


def extract_features(samples: Iterable[float], zc_threshold: float = 0.0) -> dict[str, float]:
    x = np.asarray(list(samples), dtype=float)
    if x.size == 0:
        raise ValueError("Cannot extract features from an empty window")

    centered = x - np.mean(x)

    if x.size > 1:
        previous = centered[:-1]
        current = centered[1:]
        crossed_mean = previous * current < 0
        large_enough = np.abs(current - previous) >= zc_threshold
        zc = int(np.sum(crossed_mean & large_enough))
        wl = float(np.sum(np.abs(np.diff(centered))))
    else:
        zc = 0
        wl = 0.0

    return {
        "rms": float(np.sqrt(np.mean(np.square(x)))),
        "mav": float(np.mean(np.abs(x))),
        "zc": float(zc),
        "wl": wl,
        "std": float(np.std(x)),
        "c_rms": float(np.sqrt(np.mean(np.square(centered)))),
        "c_mav": float(np.mean(np.abs(centered))),
        "c_zc": float(zc),
        "c_wl": wl,
        "c_std": float(np.std(centered)),
        "mean": float(np.mean(x)),
        "median": float(np.median(x)),
        "min": float(np.min(x)),
        "max": float(np.max(x)),
        "ptp": float(np.ptp(x)),
        "q25": float(np.percentile(x, 25)),
        "q75": float(np.percentile(x, 75)),
        "iemg": float(np.sum(np.abs(centered))),
    }


def features_row(samples: Iterable[float], zc_threshold: float = 0.0) -> list[float]:
    features = extract_features(samples, zc_threshold=zc_threshold)
    return [features[name] for name in FEATURE_COLUMNS]


def gesture_to_command(gesture: str | None) -> str:
    if gesture is None:
        return EMERGENCY_COMMAND
    return GESTURE_COMMANDS.get(gesture, EMERGENCY_COMMAND)
