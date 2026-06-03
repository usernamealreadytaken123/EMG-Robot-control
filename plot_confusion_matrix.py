from __future__ import annotations

import argparse
from html import escape
from pathlib import Path
from typing import Iterable

import joblib
import pandas as pd
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix

from emg_features import FEATURE_COLUMNS, SUPPORTED_GESTURES


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Evaluate the trained EMG gesture model and draw a confusion matrix."
    )
    parser.add_argument("--model", default="gesture_model.pkl")
    parser.add_argument("--label-encoder", default="label_encoder.pkl")
    parser.add_argument("--x-test", default="X_test.csv")
    parser.add_argument("--y-test", default="y_test.csv")
    parser.add_argument("--matrix-csv", default="confusion_matrix.csv")
    parser.add_argument("--matrix-svg", default="confusion_matrix.svg")
    parser.add_argument("--report-output", default="classification_report.txt")
    parser.add_argument("--report-svg", default="classification_report.svg")
    parser.add_argument("--title", default="Confusion matrix")
    parser.add_argument("--report-title", default="Classification metrics")
    return parser.parse_args()


def read_single_column_csv(path: Path) -> pd.Series:
    df = pd.read_csv(path)
    if df.shape[1] != 1:
        raise ValueError(f"{path} must contain exactly one column")
    return df.iloc[:, 0]


def encode_labels(values: Iterable[object], encoder) -> list[int]:
    series = pd.Series(values)
    numeric = pd.to_numeric(series, errors="coerce")
    if numeric.notna().all():
        return numeric.astype(int).tolist()

    labels = series.astype(str).str.strip().str.lower()
    return encoder.transform(labels).tolist()


def heat_color(value: int, max_value: int) -> str:
    if max_value <= 0:
        return "#f8fafc"

    intensity = value / max_value
    red = round(239 - 182 * intensity)
    green = round(246 - 132 * intensity)
    blue = round(255 - 40 * intensity)
    return f"#{red:02x}{green:02x}{blue:02x}"


def text_color(value: int, max_value: int) -> str:
    if max_value <= 0:
        return "#0f172a"
    return "#ffffff" if value / max_value >= 0.55 else "#0f172a"


def draw_confusion_matrix_svg(
    matrix: list[list[int]],
    classes: list[str],
    accuracy: float,
    output_path: Path,
    title: str,
) -> None:
    cell_size = 92
    left_margin = 132
    top_margin = 116
    right_margin = 34
    bottom_margin = 76
    n_classes = len(classes)
    width = left_margin + n_classes * cell_size + right_margin
    height = top_margin + n_classes * cell_size + bottom_margin
    max_value = max(max(row) for row in matrix) if matrix else 0

    parts: list[str] = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        "<style>",
        "text { font-family: Arial, sans-serif; }",
        ".title { font-size: 24px; font-weight: 700; fill: #0f172a; }",
        ".subtitle { font-size: 13px; fill: #475569; }",
        ".axis { font-size: 14px; font-weight: 700; fill: #334155; }",
        ".label { font-size: 13px; fill: #0f172a; }",
        ".value { font-size: 24px; font-weight: 700; }",
        ".cell { stroke: #ffffff; stroke-width: 2; }",
        "</style>",
        f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff"/>',
        f'<text x="{width / 2:.1f}" y="32" text-anchor="middle" class="title">'
        f"{escape(title)}</text>",
        f'<text x="{width / 2:.1f}" y="54" text-anchor="middle" class="subtitle">'
        f"accuracy = {accuracy:.4f} ({accuracy * 100:.2f}%)</text>",
        f'<text x="{left_margin + n_classes * cell_size / 2:.1f}" y="86" '
        f'text-anchor="middle" class="axis">Predicted gesture</text>',
        f'<text x="24" y="{top_margin + n_classes * cell_size / 2:.1f}" '
        f'text-anchor="middle" class="axis" transform="rotate(-90 24 '
        f'{top_margin + n_classes * cell_size / 2:.1f})">True gesture</text>',
    ]

    for column, class_name in enumerate(classes):
        x = left_margin + column * cell_size + cell_size / 2
        parts.append(
            f'<text x="{x:.1f}" y="{top_margin - 18}" text-anchor="middle" '
            f'class="label">{escape(class_name)}</text>'
        )

    for row, class_name in enumerate(classes):
        y = top_margin + row * cell_size + cell_size / 2 + 5
        parts.append(
            f'<text x="{left_margin - 16}" y="{y:.1f}" text-anchor="end" '
            f'class="label">{escape(class_name)}</text>'
        )

    for row in range(n_classes):
        for column in range(n_classes):
            value = int(matrix[row][column])
            x = left_margin + column * cell_size
            y = top_margin + row * cell_size
            fill = heat_color(value, max_value)
            color = text_color(value, max_value)
            parts.extend(
                [
                    f'<rect x="{x}" y="{y}" width="{cell_size}" height="{cell_size}" '
                    f'rx="8" fill="{fill}" class="cell"/>',
                    f'<text x="{x + cell_size / 2:.1f}" y="{y + cell_size / 2 + 8:.1f}" '
                    f'text-anchor="middle" class="value" fill="{color}">{value}</text>',
                ]
            )

    parts.append("</svg>")
    output_path.write_text("\n".join(parts), encoding="utf-8")


def draw_classification_report_svg(
    report: dict[str, dict[str, float] | float],
    classes: list[str],
    accuracy: float,
    output_path: Path,
    title: str,
) -> None:
    row_height = 48
    header_height = 56
    title_height = 82
    footer_height = 30
    left_margin = 28
    column_widths = [132, 112, 112, 112, 96]
    width = left_margin * 2 + sum(column_widths)
    rows = classes + ["macro avg", "weighted avg"]
    height = title_height + header_height + len(rows) * row_height + footer_height
    headers = ["Gesture", "Precision", "Recall", "F1-score", "Support"]

    def column_x(index: int) -> int:
        return left_margin + sum(column_widths[:index])

    parts: list[str] = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        "<style>",
        "text { font-family: Arial, sans-serif; }",
        ".title { font-size: 24px; font-weight: 700; fill: #0f172a; }",
        ".subtitle { font-size: 13px; fill: #475569; }",
        ".header { font-size: 13px; font-weight: 700; fill: #334155; }",
        ".cell { font-size: 14px; fill: #0f172a; }",
        ".summary { font-size: 14px; font-weight: 700; fill: #0f172a; }",
        "</style>",
        f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff"/>',
        f'<text x="{width / 2:.1f}" y="32" text-anchor="middle" class="title">'
        f"{escape(title)}</text>",
        f'<text x="{width / 2:.1f}" y="54" text-anchor="middle" class="subtitle">'
        f"accuracy = {accuracy:.4f} ({accuracy * 100:.2f}%)</text>",
    ]

    table_y = title_height
    parts.append(
        f'<rect x="{left_margin}" y="{table_y}" width="{sum(column_widths)}" '
        f'height="{header_height}" rx="8" fill="#e2e8f0"/>'
    )

    for index, header in enumerate(headers):
        x = column_x(index)
        anchor = "start" if index == 0 else "middle"
        text_x = x + 16 if index == 0 else x + column_widths[index] / 2
        parts.append(
            f'<text x="{text_x:.1f}" y="{table_y + 35}" text-anchor="{anchor}" '
            f'class="header">{escape(header)}</text>'
        )

    for row_index, row_name in enumerate(rows):
        y = title_height + header_height + row_index * row_height
        fill = "#f8fafc" if row_index % 2 == 0 else "#ffffff"
        if row_name in {"macro avg", "weighted avg"}:
            fill = "#eef2ff"

        row_data = report[row_name]
        if not isinstance(row_data, dict):
            continue

        precision = float(row_data["precision"])
        recall = float(row_data["recall"])
        f1 = float(row_data["f1-score"])
        support = int(row_data["support"])
        values = [
            row_name,
            f"{precision:.2f}",
            f"{recall:.2f}",
            f"{f1:.2f}",
            str(support),
        ]
        css_class = "summary" if row_name in {"macro avg", "weighted avg"} else "cell"

        parts.append(
            f'<rect x="{left_margin}" y="{y}" width="{sum(column_widths)}" '
            f'height="{row_height}" fill="{fill}"/>'
        )
        for index, value in enumerate(values):
            x = column_x(index)
            anchor = "start" if index == 0 else "middle"
            text_x = x + 16 if index == 0 else x + column_widths[index] / 2
            parts.append(
                f'<text x="{text_x:.1f}" y="{y + 30}" text-anchor="{anchor}" '
                f'class="{css_class}">{escape(value)}</text>'
            )

    table_height = header_height + len(rows) * row_height
    parts.append(
        f'<rect x="{left_margin}" y="{table_y}" width="{sum(column_widths)}" '
        f'height="{table_height}" rx="8" fill="none" stroke="#cbd5e1" stroke-width="1"/>'
    )
    parts.append("</svg>")
    output_path.write_text("\n".join(parts), encoding="utf-8")


def ordered_classes(encoder) -> list[str]:
    encoder_classes = [str(class_name) for class_name in encoder.classes_]
    known_order = [gesture for gesture in SUPPORTED_GESTURES if gesture in encoder_classes]
    extra_classes = [
        class_name for class_name in encoder_classes if class_name not in known_order
    ]
    return known_order + extra_classes


def main() -> None:
    args = parse_args()

    model = joblib.load(args.model)
    encoder = joblib.load(args.label_encoder)
    classes = ordered_classes(encoder)

    x_test = pd.read_csv(args.x_test)
    missing_columns = set(FEATURE_COLUMNS) - set(x_test.columns)
    if missing_columns:
        raise ValueError(f"Missing columns in {args.x_test}: {sorted(missing_columns)}")
    x_test = x_test[FEATURE_COLUMNS]

    y_test = encode_labels(read_single_column_csv(Path(args.y_test)), encoder)
    y_pred = encode_labels(model.predict(x_test), encoder)

    labels = encoder.transform(classes).tolist()
    matrix = confusion_matrix(y_test, y_pred, labels=labels)
    accuracy = accuracy_score(y_test, y_pred)

    matrix_df = pd.DataFrame(matrix, index=classes, columns=classes)
    matrix_df.index.name = "true/predicted"
    matrix_df.to_csv(args.matrix_csv)

    report = classification_report(
        y_test,
        y_pred,
        labels=labels,
        target_names=classes,
        zero_division=0,
    )
    report_dict = classification_report(
        y_test,
        y_pred,
        labels=labels,
        target_names=classes,
        zero_division=0,
        output_dict=True,
    )
    report_text = (
        f"Accuracy: {accuracy:.4f} ({accuracy * 100:.2f}%)\n\n"
        "Classification report:\n"
        f"{report}\n"
        "Confusion matrix:\n"
        f"{matrix_df.to_string()}\n"
    )
    Path(args.report_output).write_text(report_text, encoding="utf-8")

    draw_confusion_matrix_svg(
        matrix=matrix.tolist(),
        classes=classes,
        accuracy=accuracy,
        output_path=Path(args.matrix_svg),
        title=args.title,
    )
    draw_classification_report_svg(
        report=report_dict,
        classes=classes,
        accuracy=accuracy,
        output_path=Path(args.report_svg),
        title=args.report_title,
    )

    print(report_text)
    print(f"Saved matrix CSV: {args.matrix_csv}")
    print(f"Saved matrix image: {args.matrix_svg}")
    print(f"Saved metrics image: {args.report_svg}")
    print(f"Saved report: {args.report_output}")


if __name__ == "__main__":
    main()
