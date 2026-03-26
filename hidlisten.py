import hid # pip install hidapi
import time

vid = 0x303A
pid = 0x00001

try:
    while True:
        try:
            print('\n---------------------------------------')

            d = False
            for d in hid.enumerate(vid, pid):
                print(d)
                #print(f"Path: {d['path']}, Interface: {d['interface_number']}, Usage: {d['usage']}")
                if d['usage'] == 512:
                    print('Found device!')
                    break
            if not d:
                print('--- Device not found: Retrying in 2 seconds')
                time.sleep(2)
                continue

            # Open specifically by path
            h = hid.device()
            h.open_path(d['path'])
            print("Listening for ESPY HID Logs...")
            print('---------------------------------------')
            while True:
                data = h.read(65)
                if data:
                    # First byte is Report ID, rest is the string
                    message = "".join(chr(b) for b in data[1:] if b != 0)
                    print(message, end="")
        except OSError as e:
            h.close()
            print('--- OSError: Retrying in 2 seconds => ', e)
            time.sleep(2)

except KeyboardInterrupt:
    print('Exiting')
