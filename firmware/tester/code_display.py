# touchwheelsao_tester.py -- simple I2C host for TouchWheelSAO
# 5 Aug 2024 - @todbot / Tod Kurt
#


import time, board, busio
import displayio, terminalio, i2cdisplaybus
import adafruit_displayio_ssd1306
from adafruit_display_text import bitmap_label as label
from adafruit_bus_device import i2c_device

sao_i2c_addr = 0x54

sao_i2c = busio.I2C(scl=board.GP17, sda=board.GP16)
sao_i2cdev = None

dw,dh = 128,64
displayio.release_displays()
disp_i2c = busio.I2C(scl=board.GP15, sda=board.GP14)
display_bus = i2cdisplaybus.I2CDisplayBus(disp_i2c, device_address=0x3C)  # or 0x3D depending on display
display = adafruit_displayio_ssd1306.SSD1306(display_bus, width=dw, height=dh, rotation=180)
maingroup = displayio.Group()
display.root_group = maingroup
lbl_title = label.Label(terminalio.FONT, text="TouchwheelSAO tester", x=0, y=10, scale=1)
lbl_pos_text = label.Label(terminalio.FONT, text="pos:", x=0, y=30, scale=1)
lbl_pos_val = label.Label(terminalio.FONT, text="unk", x=30, y=30, scale=1)
lbl_touches_text = label.Label(terminalio.FONT, text="touches:", x=0, y=50, scale=1)
lbl_touches_val = label.Label(terminalio.FONT, text="   ", x=60, y=50, scale=1)
for l in (lbl_title, lbl_pos_text, lbl_pos_val, lbl_touches_text, lbl_touches_val):
    maingroup.append(l)


def i2c_scan(i2c):
    if i2c is None: return
    while not i2c.try_lock(): pass
    print("I2C addresses found:",
          [hex(device_address) for device_address in i2c.scan()])
    i2c.unlock()

def i2c_write_register(_i2cdev, reg_addr, reg_val):
    """Write 8 bit value to register at address."""
    if _i2cdev is None: return
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

#time.sleep(5)

led_state = 0

i2c_scan(sao_i2c)

while True:

    if sao_i2cdev is None:
        time.sleep(1)
        try: 
            sao_i2cdev = i2c_device.I2CDevice(sao_i2c, sao_i2c_addr)
        except ValueError as e:
            print("SAO error:", e)

    try:
        r = i2c_read_register_bytes(sao_i2cdev, 0, 1)
        pos = r[0]
        print("  pos:", pos)

        lbl_pos_val.text = str(pos) if pos > 0 else "no touch"

        r = i2c_read_register_bytes(sao_i2cdev, 1, 1)
        print("  touches:", r, f'{r[0]:03b}')
        lbl_touches_val.text = f'{r[0]:03b}'
        
    except Exception as e:
        print("SAO no device:", e)
        lbl_pos_val.text = "no SAO"
        sao_i2cdev = None
        
    time.sleep(0.2)
        
        
    # try:
    #     print("%.2f write reg:" % time.monotonic())
    #     i2c_write_register(sao_i2cdev, 14, led_state)
    #     led_state = not led_state
    #     #i2c_write_register(i2cdev, 17, 100)  # set RGB B 
    #     print("reading...:")
    #     r = i2c_read_register_bytes(sao_i2cdev, 0, 1)
    #     print("  pos:", r, r[0])

    #     lbl_pos_val.text = str(r[0])
        
    #     r = i2c_read_register_bytes(sao_i2cdev, 1, 1)
    #     print("  touches:", r, f'{r[0]:03b}')
    #     r = i2c_read_register_bytes(sao_i2cdev, 2, 1) # RAW0L
    #     print("  raw0L:", r, r[0])
    #     r = i2c_read_register_bytes(sao_i2cdev, 3, 1) # RAW0H
    #     print("  raw0H:", r, r[0])
    #     r = i2c_read_register_bytes(sao_i2cdev, 8, 1) # THRESH0L
    #     print("  thresh0L:", r, r[0])
        
    # except OSError as e:
    #     print(e)
    # time.sleep(0.2)






