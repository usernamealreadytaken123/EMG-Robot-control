from __future__ import annotations

import argparse
import time
from collections import deque

import joblib
import pandas as pd
import serial

from emg_features import (
    EMERGENCY_COMMAND,
    FEATURE_COLUMNS,
    GESTURE_COMMANDS,
    extract_features,
    gesture_to_command,
)


DEFAULT_PORT = "COM7"
DEFAULT_BAUD = 115200


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Recognize EMG gestures online and send ROV commands to Arduino."
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--model", default="gesture_model.pkl")
    parser.add_argument("--label-encoder", default="label_encoder.pkl")
    parser.add_argument("--window-seconds", type=float, default=1.0)
    parser.add_argument("--step-seconds", type=float, default=0.125)
    parser.add_argument("--min-samples", type=int, default=40)
    parser.add_argument("--stable-count", type=int, default=3)
    parser.add_argument("--min-confidence", type=float, default=0.55)
    parser.add_argument("--command-repeat-seconds", type=float, default=0.25)
    parser.add_argument("--no-sample-stop-seconds", type=float, default=1.5)
    parser.add_argument("--zc-threshold", type=float, default=0.0)
    return parser.parse_args()


def read_adc_line(port: serial.Serial) -> int | None:
    raw_line = port.readline()
    if not raw_line:
        return None

    text = raw_line.decode("ascii", errors="ignore").strip()
    if not text:
        return None

    try:
        value = int(text)
    except ValueError:
        return None

    if 0 <= value <= 1023:
        return value
    return None


def predict_gesture(
    model,
    encoder,
    samples: list[float],
    min_confidence: float,
    zc_threshold: float,
) -> tuple[str | None, float | None]:
    features = extract_features(samples, zc_threshold=zc_threshold)
    features_df = pd.DataFrame([[features[name] for name in FEATURE_COLUMNS]], columns=FEATURE_COLUMNS)

    encoded_prediction = model.predict(features_df)[0]
    gesture = encoder.inverse_transform([encoded_prediction])[0]

    confidence = None
    if hasattr(model, "predict_proba"):
        probabilities = model.predict_proba(features_df)[0]
        confidence = float(max(probabilities))
        if confidence < min_confidence:
            return None, confidence

    if gesture not in GESTURE_COMMANDS:
        return None, confidence

    return gesture, confidence


def send_command(port: serial.Serial, command: str) -> None:
    port.write(command.encode("ascii"))
    port.flush()


def main() -> None:
    args = parse_args()
    model = joblib.load(args.model)
    encoder = joblib.load(args.label_encoder)

    samples: deque[tuple[float, int]] = deque()
    stable_history: deque[str] = deque(maxlen=args.stable_count)
    last_prediction_at = 0.0
    last_command: str | None = None
    last_command_sent_at = 0.0
    last_sample_received_at = time.time()
    last_no_sample_stop_at = 0.0

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        time.sleep(2)
        ser.reset_input_buffer()
        print(f"Online recognition started on {args.port} at {args.baud} baud.")
        print("Incoming Arduino format: Serial.println(analogRead(A0));")
        print("Outgoing commands: relax=S, fist=F, open=B, left=L, right=R")
        print("Press Ctrl+C to stop.")

        try:
            while True:
                value = read_adc_line(ser)
                if value is None:
                    now = time.time()
                    if (
                        now - last_sample_received_at >= args.no_sample_stop_seconds
                        and now - last_no_sample_stop_at >= args.no_sample_stop_seconds
                    ):
                        send_command(ser, "D")
                        last_command = "D"
                        last_command_sent_at = now
                        last_no_sample_stop_at = now
                        print("No EMG samples. Emergency stop/disarm sent.")
                    continue

                now = time.time()
                last_sample_received_at = now
                samples.append((now, value))

                while samples and now - samples[0][0] > args.window_seconds:
                    samples.popleft()

                if len(samples) < args.min_samples:
                    continue

                if now - last_prediction_at < args.step_seconds:
                    continue
                last_prediction_at = now

                window_values = [sample for _, sample in samples]
                gesture, confidence = predict_gesture(
                    model,
                    encoder,
                    window_values,
                    min_confidence=args.min_confidence,
                    zc_threshold=args.zc_threshold,
                )

                if gesture is None:
                    stable_history.clear()
                    should_send = (
                        last_command != EMERGENCY_COMMAND
                        or now - last_command_sent_at >= args.command_repeat_seconds
                    )
                    if should_send:
                        send_command(ser, EMERGENCY_COMMAND)
                        last_command = EMERGENCY_COMMAND
                        last_command_sent_at = now
                    confidence_text = "n/a" if confidence is None else f"{confidence:.2f}"
                    status = "command=S" if should_send else "command=S held"
                    print(f"gesture=unknown confidence={confidence_text} {status}")
                    continue

                stable_history.append(gesture)
                confidence_text = "n/a" if confidence is None else f"{confidence:.2f}"

                if len(stable_history) == args.stable_count and len(set(stable_history)) == 1:
                    command = gesture_to_command(gesture)
                    should_send = (
                        command != last_command
                        or now - last_command_sent_at >= args.command_repeat_seconds
                    )
                    if should_send:
                        send_command(ser, command)
                        last_command = command
                        last_command_sent_at = now
                        print(
                            f"gesture={gesture} confidence={confidence_text} command={command}"
                        )
                    else:
                        print(
                            f"gesture={gesture} confidence={confidence_text} command={command} held"
                        )
                else:
                    print(f"gesture={gesture} confidence={confidence_text} waiting")

        except KeyboardInterrupt:
            if last_command != EMERGENCY_COMMAND:
                send_command(ser, EMERGENCY_COMMAND)
            send_command(ser, "D")
            print("Stopped. Emergency command S sent.")


if __name__ == "__main__":
    main()
