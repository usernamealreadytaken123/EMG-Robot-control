from __future__ import annotations

import argparse
import time

import serial


DEFAULT_PORT = "COM7"
DEFAULT_BAUD = 115200
VALID_COMMANDS = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send exact control bytes to Arduino without EMG recognition."
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument(
        "--sequence",
        default="R:2,S:5,R:2,S:5",
        help="Comma-separated COMMAND:SECONDS items, for example R:2,S:5.",
    )
    parser.add_argument(
        "--repeat-seconds",
        type=float,
        default=0.2,
        help="How often to resend the current command during each sequence step.",
    )
    parser.add_argument(
        "--show-input",
        action="store_true",
        help="Print non-empty lines received from Arduino while the test runs.",
    )
    parser.add_argument(
        "--startup-wait",
        type=float,
        default=5.0,
        help="Seconds to wait after opening Serial before sending commands.",
    )
    return parser.parse_args()


def parse_sequence(text: str) -> list[tuple[str, float]]:
    steps: list[tuple[str, float]] = []
    for raw_item in text.split(","):
        item = raw_item.strip()
        if not item:
            continue

        command_text, separator, seconds_text = item.partition(":")
        if separator != ":":
            raise ValueError(f"Bad sequence item {item!r}. Use COMMAND:SECONDS.")

        command = command_text.strip().upper()
        if command not in VALID_COMMANDS:
            raise ValueError(f"Bad command {command!r}. Use uppercase A-Z or 0-9.")

        seconds = float(seconds_text)
        if seconds <= 0:
            raise ValueError("Step duration must be positive.")

        steps.append((command, seconds))

    if not steps:
        raise ValueError("Sequence is empty.")

    return steps


def drain_input(port: serial.Serial, show_input: bool) -> None:
    while port.in_waiting:
        raw_line = port.readline()
        if show_input:
            text = raw_line.decode("ascii", errors="ignore").strip()
            if text:
                print(f"RX {text}")


def send_command(port: serial.Serial, command: str) -> None:
    port.write(command.encode("ascii"))
    port.flush()
    print(f"TX {command}")


def main() -> None:
    args = parse_args()
    sequence = parse_sequence(args.sequence)

    with serial.Serial(args.port, args.baud, timeout=0.05) as ser:
        time.sleep(args.startup_wait)
        ser.reset_input_buffer()
        print(f"Manual command test started on {args.port} at {args.baud} baud.")
        print(f"Startup wait: {args.startup_wait:.2f}s")
        print(f"Sequence: {args.sequence}")

        for command, duration in sequence:
            step_end = time.time() + duration
            print(f"STEP {command} for {duration:.2f}s")

            while time.time() < step_end:
                drain_input(ser, args.show_input)
                send_command(ser, command)

                wait_end = min(step_end, time.time() + args.repeat_seconds)
                while time.time() < wait_end:
                    drain_input(ser, args.show_input)
                    time.sleep(0.01)

        send_command(ser, "S")
        time.sleep(0.2)
        send_command(ser, "D")
        print("Done. Final S and D sent.")


if __name__ == "__main__":
    main()
