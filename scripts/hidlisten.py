import argparse
import hid  # pip install hidapi
import time


def pick_device(devices, vendor_usage_page):
    for d in devices:
        if d.get("usage_page") == vendor_usage_page:
            return d
    return devices[0] if devices else None


def run(vid, pid, vendor_usage_page, duration_s=None):
    start = time.time()
    print_not_found = True
    while True:
        if print_not_found: print("\n---------------------------------------")
        h = None
        try:
            devices = hid.enumerate(vid, pid)
            for d in devices:
                print(d)

            selected = pick_device(devices, vendor_usage_page)
            if selected is None:
                if print_not_found: print("--- Device not found: Retrying")
                else: print('.', end="", flush=True)
                time.sleep(0.2)
                print_not_found = False
                continue

            print_not_found = True
            h = hid.device()
            h.open_path(selected["path"])
            h.set_nonblocking(1)
            print(f"Opened path: {selected['path']!r}")
            print("Listening for loom.a.ne HID Logs...")
            print("---------------------------------------")

            idle = 0
            while True:
                data = h.read(64)
                if data:
                    idle = 0
                    payload = data[1:] if data and data[0] in (1, 2, 3, 4) else data
                    msg = "".join(chr(b) for b in payload if b != 0)
                    if msg:
                        print(msg, end="", flush=True)
                    else:
                        print(f"<{len(data)} raw bytes>", end="", flush=True)
                else:
                    idle += 1
                    if idle % 200 == 0:
                        pass# print(".", end="", flush=True)
                    time.sleep(0.005)

                if duration_s is not None and (time.time() - start) >= duration_s:
                    return 0
        except OSError as e:
            if h is not None:
                h.close()
            print("--- OSError: Retrying => ", e)
            time.sleep(0.2)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--vid", type=lambda x: int(x, 0), default=0x303A)
    parser.add_argument("--pid", type=lambda x: int(x, 0), default=0x80C4)
    parser.add_argument("--usage-page", type=lambda x: int(x, 0), default=0xFFAB)
    parser.add_argument("--duration", type=float, default=None, help="Optional auto-exit duration in seconds")
    args = parser.parse_args()

    try:
        raise SystemExit(run(args.vid, args.pid, args.usage_page, args.duration))
    except KeyboardInterrupt:
        print("Exiting")
