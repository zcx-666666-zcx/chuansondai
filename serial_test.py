import time

import serial

SERIAL_PORT = "/dev/cu.usbserial-14220"
BAUDRATE = 9600

set = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
time.sleep(2)

print("send R")
set.write(b"R")

time.sleep(0.5)

data = set.read_all()
print("reply:", data.decode("utf-8", errors="ignore"))

set.close()
