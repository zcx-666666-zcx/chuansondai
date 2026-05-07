import time
import serial

SERIAL_PORT = "/dev/cu.usbserial-14220"
BAUDRATE = 9600

ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
time.sleep(2)

print("send R")
ser.write(b"R")

time.sleep(0.5)

data = ser.read_all()
print("reply:", data.decode("utf-8", errors="ignore"))

ser.close()