from __future__ import annotations

import argparse
import csv
import time
from pathlib import Path

import serial

from emg_features import SUPPORTED_GESTURES


DEFAULT_PORT = "COM7"
DEFAULT_BAUD = 115200
DEFAULT_OUTPUT = "emg_labeled_data.csv"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect EMG samples from Arduino Serial.println(analogRead(A0))."
    )
    parser.add_argument("--port", default=DEFAULT_PORT, help="Arduino serial port")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Serial baud rate")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Output CSV file")
    parser.add_argument(
        "--gesture",
        choices=SUPPORTED_GESTURES,
        help="Gesture label for this recording session",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Optional recording duration in seconds; 0 means until Ctrl+C",
    )
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


def ensure_header(path: Path) -> bool:
    return not path.exists() or path.stat().st_size == 0


def main() -> None:
    args = parse_args()
    gesture = args.gesture
    if gesture is None:
        options = ", ".join(SUPPORTED_GESTURES)
        gesture = input(f"Gesture label ({options}): ").strip().lower()

    if gesture not in SUPPORTED_GESTURES:
        raise SystemExit(
            f"Unsupported gesture '{gesture}'. Use one of: {', '.join(SUPPORTED_GESTURES)}"
        )

    output_path = Path(args.output)
    write_header = ensure_header(output_path)

    with serial.Serial(args.port, args.baud, timeout=1) as ser, output_path.open(
        "a", newline="", encoding="utf-8"
    ) as file:
        time.sleep(2)
        ser.reset_input_buffer()

        writer = csv.writer(file)
        if write_header:
            writer.writerow(["timestamp", "value", "gesture"])

        print(
            f"Recording '{gesture}' from {args.port} at {args.baud} baud into {output_path}."
        )
        print("Arduino format must be: Serial.println(analogRead(A0));")
        print("Press Ctrl+C to stop.")

        started_at = time.time()
        samples_written = 0

        try:
            while True:
                if args.duration > 0 and time.time() - started_at >= args.duration:
                    break

                value = read_adc_line(ser)
                if value is None:
                    continue

                timestamp = time.time()
                writer.writerow([f"{timestamp:.6f}", value, gesture])
                samples_written += 1

                if samples_written % 50 == 0:
                    file.flush()
                    print(f"{samples_written} samples written; last value={value}")

        except KeyboardInterrupt:
            pass
        finally:
            file.flush()
            print(f"Stopped. Total samples written: {samples_written}")


if __name__ == "__main__":
    main()
