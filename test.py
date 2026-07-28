import serial
import time

# Protocol constants
FRAME_SOF = 0xE7
FRAME_EOF = 0x7E

# Commands
CMD_ECHO = 0
CMD_SYSTEM_INFO = 1
CMD_GET_STATUS = 2
CMD_START_UPDATE = 3
CMD_FLASH_ERASE = 4


def crc8_update(crc, byte):
    """CRC-8 calculation, matching the C code (polynomial 0x07)."""
    crc ^= byte
    for _ in range(8):
        if crc & 0x80:
            crc = ((crc << 1) ^ 0x07) & 0xFF
        else:
            crc = (crc << 1) & 0xFF
    return crc


def calculate_crc(cmd_id, size, payload):
    """Calculate CRC for the entire frame."""
    crc = crc8_update(0, cmd_id)
    crc = crc8_update(crc, size)
    for byte in payload:
        crc = crc8_update(crc, byte)
    return crc


def build_frame(cmd_id, payload=b""):
    """Build a frame for transmission."""
    size = len(payload)
    if size > 64:
        raise ValueError("Payload size exceeds MAX_PAYLOAD (64)")

    crc = calculate_crc(cmd_id, size, payload)

    frame = bytearray([FRAME_SOF, cmd_id, size])
    frame.extend(payload)
    frame.extend([crc, FRAME_EOF])
    return frame


def send_and_receive(ser, cmd_id, payload=b"", read_timeout=0.5):
    """Send command and wait for a response, reading until FRAME_EOF or timeout."""
    frame = build_frame(cmd_id, payload)
    print(f"Sending:  {frame.hex(' ').upper()}")

    ser.reset_input_buffer()
    ser.write(frame)

    deadline = time.time() + read_timeout
    buf = bytearray()
    while time.time() < deadline:
        if ser.in_waiting > 0:
            buf.extend(ser.read(ser.in_waiting))
            if buf and buf[-1] == FRAME_EOF:
                break
        else:
            time.sleep(0.01)

    if buf:
        print(f"Received: {buf.hex(' ').upper()}")
    else:
        print("No response from the device.")

    return bytes(buf)


def main():
    PORT = '/dev/cu.usbmodem1234561'
    BAUD = 115200

    try:
        with serial.Serial(PORT, BAUD, timeout=1) as ser:
            # Explicitly raise DTR — required for the firmware's
            # tud_cdc_line_state_cb() to enable sending responses
            ser.dtr = True
            time.sleep(0.3)  # give the MCU time to process line_state_cb / send banner

            print(f"Connected to {PORT} at {BAUD} baud")

            # Drain the startup banner, if any
            if ser.in_waiting:
                banner = ser.read(ser.in_waiting)
                print(f"Banner: {banner}")

            # 1. Test CMD_ECHO command
            print("\n--- Testing CMD_ECHO ---")
            send_and_receive(ser, CMD_ECHO, b"Hello")

            # 2. Test CMD_SYSTEM_INFO command
            print("\n--- Testing CMD_SYSTEM_INFO ---")
            send_and_receive(ser, CMD_SYSTEM_INFO)

            # 3. Test CMD_START_UPDATE command
            print("\n--- Testing CMD_START_UPDATE ---")
            send_and_receive(ser, CMD_START_UPDATE)

            # 4. Test CMD_FLASH_ERASE command
            print("\n--- Testing CMD_FLASH_ERASE ---")
            send_and_receive(ser, CMD_FLASH_ERASE)

    except serial.SerialException as e:
        print(f"Port connection error: {e}")


if __name__ == "__main__":
    main()