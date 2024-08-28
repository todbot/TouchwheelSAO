# touchwheelsao_tester.py -- simple I2C host for TouchWheelSAO
# 5 Aug 2024 - @todbot / Tod Kurt
#


import time, board, busio
from adafruit_bus_device import i2c_device

i2c_addr = 0x54
i2c = busio.I2C(board.GP17, board.GP16)
i2cdev = i2c_device.I2CDevice(i2c, i2c_addr)

def i2c_scan(i2c):
    while not i2c.try_lock(): pass
    print("I2C addresses found:",
          [hex(device_address) for device_address in i2c.scan()])
    i2c.unlock()

def i2c_write_register(_i2cdev, reg_addr, reg_val):
    """Write 8 bit value to register at address."""
    _buf = bytearray(2)
    _buf[0] = reg_addr
    _buf[1] = reg_val
    with _i2cdev as myi2cdev:
        myi2cdev.write(_buf)

def i2c_read_register_bytes(_i2cdev, reg_addr, num_bytes):
    with _i2cdev as myi2cdev:
        myi2cdev.write(bytes([reg_addr]))
        result = bytearray(num_bytes)
        myi2cdev.readinto(result)
    return result


led_state = 0

while True:
    i2c_scan(i2c)
    try:
        print("%.2f write reg:" % time.monotonic())
        i2c_write_register(i2cdev, 14, led_state)
        led_state = not led_state
        #i2c_write_register(i2cdev, 17, 100)  # set RGB B 
        print("reading...:")
        r = i2c_read_register_bytes(i2cdev, 0, 1)
        print("  pos:", r, r[0])
        r = i2c_read_register_bytes(i2cdev, 1, 1)
        print("  touches:", r, f'{r[0]:03b}')
        r = i2c_read_register_bytes(i2cdev, 2, 1) # RAW0L
        print("  raw0L:", r, r[0])
        r = i2c_read_register_bytes(i2cdev, 3, 1) # RAW0H
        print("  raw0H:", r, r[0])
        r = i2c_read_register_bytes(i2cdev, 8, 1) # THRESH0L
        print("  thresh0L:", r, r[0])
        
    except OSError as e:
        print(e)
    time.sleep(1)



