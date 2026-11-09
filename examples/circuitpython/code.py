"""TouchwheelSAO demo: rainbow spin, touch angle, and a scroll counter"""

import time
import board

from touchwheel import Touchwheel, color_wheel

# Or busio.I2C(scl=..., sda=...) for your board's SAO pins
i2c = board.I2C()
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
        hue = int(time.monotonic() * 1000) // 6
        wheel.rgbs([color_wheel(hue + n * 85) for n in range(3)])

    if not wheel.present:
        print("no touchwheel found")
        time.sleep(1)
    time.sleep(0.04)
