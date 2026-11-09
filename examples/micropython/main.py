"""TouchwheelSAO demo: rainbow spin, touch angle, and a scroll counter"""

import time
from machine import I2C, Pin

from touchwheel import Touchwheel, color_wheel

# Change to your board's SAO / I2C pins
i2c = I2C(0, scl=Pin(18), sda=Pin(17), freq=400_000)
wheel = Touchwheel(i2c)

scroll = 0
was_touched = False
while True:
    angle = wheel.angle()
    scroll += wheel.delta()
    touched = angle is not None

    if touched != was_touched:
        was_touched = touched
        wheel.status_led(touched)
        if not touched:
            print("released, scroll:", scroll)

    if touched:
        # The wheel lights itself to match the touch, so leave the LEDs be
        print("angle:", angle, "scroll:", scroll)
    else:
        hue = time.ticks_ms() // 6
        wheel.rgbs([color_wheel(hue + n * 85) for n in range(3)])

    if not wheel.present:
        print("no touchwheel found")
        time.sleep(1)
    time.sleep_ms(40)
