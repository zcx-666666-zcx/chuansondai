import argparse
import time

import serial


DEFAULT_PORT = "/dev/cu.usbserial-14220"
DEFAULT_BAUDRATE = 9600
DEFAULT_SEND_SECONDS = 5.0
DEFAULT_STOP_SECONDS = 5.0
DEFAULT_INTERVAL_SECONDS = 0.05


def parse_args():
    parser = argparse.ArgumentParser(
        description="Continuously send R for a period, then pause for a period."
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE)
    parser.add_argument("--send-seconds", type=float, default=DEFAULT_SEND_SECONDS)
    parser.add_argument("--stop-seconds", type=float, default=DEFAULT_STOP_SECONDS)
    parser.add_argument("--interval", type=float, default=DEFAULT_INTERVAL_SECONDS)
    parser.add_argument(
        "--cycles",
        type=int,
        default=0,
        help="0 means run forever. Use 1 to send once and stop once.",
    )
    return parser.parse_args()


def send_for_duration(ser, duration_s, interval_s):
    start = time.monotonic()
    count = 0

    while time.monotonic() - start < duration_s:
        ser.write(b"R")
        ser.flush()
        count += 1

        rx = ser.read_all()
        if rx:
            print(f"[RX] {rx!r}")

        time.sleep(interval_s)

    return count


def main():
    args = parse_args()

    print(f"Opening serial: {args.port} @ {args.baudrate}")
    with serial.Serial(args.port, args.baudrate, timeout=0.1) as ser:
        time.sleep(2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print("Serial opened. Press Ctrl+C to exit.")

        cycle = 0
        while args.cycles == 0 or cycle < args.cycles:
            cycle += 1
            print(f"[cycle {cycle}] sending R for {args.send_seconds:.1f}s")
            sent = send_for_duration(ser, args.send_seconds, args.interval)
            print(f"[cycle {cycle}] sent {sent} bytes, pausing {args.stop_seconds:.1f}s")
            time.sleep(args.stop_seconds)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
