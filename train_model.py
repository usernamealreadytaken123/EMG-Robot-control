from __future__ import annotations

import argparse

import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder

from emg_features import FEATURE_COLUMNS


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train RandomForest EMG gesture model.")
    parser.add_argument("--features", default="features_dataset.csv")
    parser.add_argument("--model-output", default="gesture_model.pkl")
    parser.add_argument("--label-encoder-output", default="label_encoder.pkl")
    parser.add_argument("--x-train", default="X_train.csv")
    parser.add_argument("--x-test", default="X_test.csv")
    parser.add_argument("--y-train", default="y_train.csv")
    parser.add_argument("--y-test", default="y_test.csv")
    parser.add_argument("--test-size", type=float, default=0.2)
    parser.add_argument("--random-state", type=int, default=42)
    parser.add_argument("--n-estimators", type=int, default=300)
    return parser.parse_args()


def can_stratify(y_encoded: pd.Series, test_size: float) -> bool:
    class_counts = y_encoded.value_counts()
    if len(class_counts) < 2 or class_counts.min() < 2:
        return False
    test_count = int(round(len(y_encoded) * test_size))
    return test_count >= len(class_counts)


def main() -> None:
    args = parse_args()
    df = pd.read_csv(args.features)

    missing_columns = set(FEATURE_COLUMNS + ["gesture"]) - set(df.columns)
    if missing_columns:
        raise ValueError(f"Missing columns in {args.features}: {sorted(missing_columns)}")

    X = df[FEATURE_COLUMNS]
    gestures = df["gesture"].astype(str).str.strip().str.lower()

    encoder = LabelEncoder()
    y_encoded = pd.Series(encoder.fit_transform(gestures))

    stratify = y_encoded if can_stratify(y_encoded, args.test_size) else None
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y_encoded,
        test_size=args.test_size,
        random_state=args.random_state,
        stratify=stratify,
    )

    model = RandomForestClassifier(
        n_estimators=args.n_estimators,
        random_state=args.random_state,
        class_weight="balanced",
    )
    model.fit(X_train, y_train)

    joblib.dump(model, args.model_output)
    joblib.dump(encoder, args.label_encoder_output)

    X_train.to_csv(args.x_train, index=False)
    X_test.to_csv(args.x_test, index=False)
    y_train.to_csv(args.y_train, index=False)
    y_test.to_csv(args.y_test, index=False)

    y_pred = model.predict(X_test)
    print(f"Saved model: {args.model_output}")
    print(f"Saved label encoder: {args.label_encoder_output}")
    print("Classes:", ", ".join(encoder.classes_))
    print("\nClassification report:")
    print(
        classification_report(
            y_test,
            y_pred,
            labels=list(range(len(encoder.classes_))),
            target_names=encoder.classes_,
            zero_division=0,
        )
    )
    print("Confusion matrix:")
    print(confusion_matrix(y_test, y_pred, labels=list(range(len(encoder.classes_)))))


if __name__ == "__main__":
    main()
