from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder

from emg_features import FEATURE_COLUMNS, SUPPORTED_GESTURES, features_row


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare windowed EMG feature dataset.")
    parser.add_argument("--input", default="emg_labeled_data.csv", help="Input labeled CSV")
    parser.add_argument(
        "--features-output",
        default="features_dataset.csv",
        help="Output CSV with extracted features",
    )
    parser.add_argument("--x-train", default="X_train.csv")
    parser.add_argument("--x-test", default="X_test.csv")
    parser.add_argument("--y-train", default="y_train.csv")
    parser.add_argument("--y-test", default="y_test.csv")
    parser.add_argument("--label-encoder", default="label_encoder.pkl")
    parser.add_argument("--window-seconds", type=float, default=1.0)
    parser.add_argument("--step-seconds", type=float, default=0.125)
    parser.add_argument("--min-samples", type=int, default=40)
    parser.add_argument("--max-gap-seconds", type=float, default=2.0)
    parser.add_argument("--zc-threshold", type=float, default=0.0)
    parser.add_argument("--test-size", type=float, default=0.2)
    parser.add_argument("--random-state", type=int, default=42)
    return parser.parse_args()


def load_labeled_data(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    expected_columns = {"timestamp", "value", "gesture"}
    missing_columns = expected_columns - set(df.columns)
    if missing_columns:
        raise ValueError(f"Missing columns in {path}: {sorted(missing_columns)}")

    df = df[["timestamp", "value", "gesture"]].copy()
    df["timestamp"] = pd.to_numeric(df["timestamp"], errors="coerce")
    df["value"] = pd.to_numeric(df["value"], errors="coerce")
    df["gesture"] = df["gesture"].astype(str).str.strip().str.lower()
    df = df.dropna(subset=["timestamp", "value", "gesture"])

    unknown = sorted(set(df["gesture"]) - set(SUPPORTED_GESTURES))
    if unknown:
        raise ValueError(
            "Unsupported gestures in dataset: "
            f"{unknown}. Supported: {list(SUPPORTED_GESTURES)}"
        )

    return df.sort_values("timestamp").reset_index(drop=True)


def add_segments(df: pd.DataFrame, max_gap_seconds: float) -> pd.DataFrame:
    df = df.copy()
    gesture_changed = df["gesture"].ne(df["gesture"].shift())
    time_gap = df["timestamp"].diff().gt(max_gap_seconds).fillna(False)
    df["segment_id"] = (gesture_changed | time_gap).cumsum()
    return df


def extract_windowed_features(args: argparse.Namespace, df: pd.DataFrame) -> pd.DataFrame:
    rows: list[dict[str, float | str]] = []
    segmented = add_segments(df, max_gap_seconds=args.max_gap_seconds)

    for _, segment in segmented.groupby("segment_id", sort=False):
        gesture = segment["gesture"].iloc[0]
        start_time = float(segment["timestamp"].min())
        end_time = float(segment["timestamp"].max())

        window_start = start_time
        while window_start + args.window_seconds <= end_time:
            window_end = window_start + args.window_seconds
            window = segment[
                (segment["timestamp"] >= window_start)
                & (segment["timestamp"] < window_end)
            ]

            if len(window) >= args.min_samples:
                feature_values = features_row(
                    window["value"].to_numpy(), zc_threshold=args.zc_threshold
                )
                row = dict(zip(FEATURE_COLUMNS, feature_values))
                row["gesture"] = gesture
                rows.append(row)

            window_start += args.step_seconds

    if not rows:
        raise ValueError(
            "No feature windows were created. Try reducing --min-samples or "
            "recording longer gesture sessions."
        )

    return pd.DataFrame(rows, columns=FEATURE_COLUMNS + ["gesture"])


def can_stratify(y_encoded: pd.Series, test_size: float) -> bool:
    class_counts = y_encoded.value_counts()
    if len(class_counts) < 2 or class_counts.min() < 2:
        return False
    test_count = int(round(len(y_encoded) * test_size))
    return test_count >= len(class_counts)


def main() -> None:
    args = parse_args()

    labeled_df = load_labeled_data(Path(args.input))
    features_df = extract_windowed_features(args, labeled_df)
    features_df.to_csv(args.features_output, index=False)

    encoder = LabelEncoder()
    y_encoded = pd.Series(encoder.fit_transform(features_df["gesture"]))
    joblib.dump(encoder, args.label_encoder)

    X = features_df[FEATURE_COLUMNS]
    stratify = y_encoded if can_stratify(y_encoded, args.test_size) else None
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y_encoded,
        test_size=args.test_size,
        random_state=args.random_state,
        stratify=stratify,
    )

    X_train.to_csv(args.x_train, index=False)
    X_test.to_csv(args.x_test, index=False)
    y_train.to_csv(args.y_train, index=False)
    y_test.to_csv(args.y_test, index=False)

    print(f"Prepared {len(features_df)} windows from {len(labeled_df)} raw samples.")
    print(f"Feature columns: {', '.join(FEATURE_COLUMNS)}")
    print("Gesture counts:")
    print(features_df["gesture"].value_counts().sort_index().to_string())


if __name__ == "__main__":
    main()
