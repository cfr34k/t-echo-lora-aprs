#!/usr/bin/env python3

import sys

from bluepy import btle
import time

class MyDelegate(btle.DefaultDelegate):
    def __init__(self):
        btle.DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        timestr = time.strftime("%F %H:%M:%S")
        print(f"\nReceived notification ({len(data)} bytes) at {timestr}:\n{data}")

print("Connecting... ", end='', flush=True)
device = btle.Peripheral(sys.argv[1], addrType=btle.ADDR_TYPE_RANDOM)
device.setMTU(247)
print("OK!")

device.setDelegate( MyDelegate() )

service      = device.getServiceByUUID("00000001-b493-bb5d-2a6a-4682945c9e00")
message_char = service.getCharacteristics("00000103-b493-bb5d-2a6a-4682945c9e00")[0] # received message

device.writeCharacteristic(message_char.valHandle+1, b"\x01\x00")

while True:
    if device.waitForNotifications(1.0):
        # handleNotification() was called
        continue

    print(".", end='', flush=True)
