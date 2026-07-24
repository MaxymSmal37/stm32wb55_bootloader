import serial
import time

# Protocol constants
FRAME_SOF = 0xE7
FRAME_EOF = 0x37

# Commands
CMD_ECHO = 0
CMD_SYSTEM_INFO = 1
CMD_GET_STATUS = 2

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

def send_and_receive(ser, cmd_id, payload=b""):
    """Send command and wait for a response."""
    frame = build_frame(cmd_id, payload)
    print(f"Sending:  {frame.hex(' ').upper()}")
    ser.write(frame)
    
    # Wait a bit for processing (depends on MCU speed)
    time.sleep(0.1)
    
    # Read all available bytes
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"Received: {response.hex(' ').upper()}")
        return response
    else:
        print("No response from the device.")
        return None

def main():
    PORT = '/dev/tty.usbserial-A50285BI' 
    BAUD = 115200
    
    try:
        with serial.Serial(PORT, BAUD, timeout=1) as ser:
            print(f"Connected to {PORT} at {BAUD} baud")
            
            # 1. Test CMD_ECHO command
            print("\n--- Testing CMD_ECHO ---")
            send_and_receive(ser, CMD_ECHO, b"Hello")
            
            # 2. Test CMD_SYSTEM_INFO command
            print("\n--- Testing CMD_SYSTEM_INFO ---")
            send_and_receive(ser, CMD_SYSTEM_INFO)
            
    except serial.SerialException as e:
        print(f"Port connection error: {e}")

if __name__ == "__main__":
    main()


