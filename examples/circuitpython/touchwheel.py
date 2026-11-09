"""TouchwheelSAO driver for MicroPython and CircuitPython

Pass in a MicroPython `machine.I2C` / `SoftI2C`, or a CircuitPython
`busio.I2C` / `board.I2C()`. Every call is a no-op (or returns None) when
no wheel answers, and the wheel is found again if it's plugged back in.
"""

import time

try:
    from time import monotonic
except ImportError:  # MicroPython
    def monotonic():
        return time.ticks_ms() / 1000

ADDR = 0x54
_REG_POSITION = 0
_REG_TOUCHES = 1
_REG_LED_STATUS = 14
_REG_LED_RGB = 15
_REG_LED0 = 18
_REG_VERSION = 27
# Also covers the wheel's ~2 sec startup animation
_RETRY_SECS = 2


class Touchwheel:
    def __init__(self, i2c, addr=ADDR):
        self.i2c = i2c
        self.addr = addr
        self.version = None
        self._mpy = hasattr(i2c, "writeto_mem")
        self._found = False
        self._last_try = None
        self._last_pos = None

    # CircuitPython's I2C must be locked around each use
    def _lock(self):
        if not self._mpy:
            while not self.i2c.try_lock():
                pass

    def _unlock(self):
        if not self._mpy:
            self.i2c.unlock()

    def _write(self, reg, buf):
        self._lock()
        try:
            if self._mpy:
                self.i2c.writeto_mem(self.addr, reg, buf)
            else:
                self.i2c.writeto(self.addr, bytes([reg]) + buf)
        finally:
            self._unlock()

    # Multi-byte reads only get the first register right, so one at a time
    def _read(self, reg):
        self._lock()
        try:
            if self._mpy:
                return self.i2c.readfrom_mem(self.addr, reg, 1)[0]
            buf = bytearray(1)
            self.i2c.writeto_then_readfrom(self.addr, bytes([reg]), buf)
            return buf[0]
        finally:
            self._unlock()

    def _scan(self):
        self._lock()
        try:
            return self.i2c.scan()
        finally:
            self._unlock()

    @property
    def present(self):
        """True if a wheel answered last time we talked to it"""
        return self._found

    def _available(self):
        if self._found:
            return True
        now = monotonic()
        # 0 <= also retries if MicroPython's ticks_ms wraps
        if self._last_try is not None and 0 <= now - self._last_try < _RETRY_SECS:
            return False
        self._last_try = now
        try:
            if self.addr not in self._scan():
                return False
            # v1 firmware has no version reg and returns junk here
            v = self._read(_REG_VERSION)
        except OSError:
            return False
        self.version = v if v >= 2 else 1
        self._found = True
        return True

    def _call(self, fn, *args):
        if not self._available():
            return None
        try:
            return fn(*args)
        except OSError:
            self._found = False
            return None

    def rgb(self, r, g, b):
        """Set all 3 LEDs to one color, each 0-255"""
        self._call(self._write, _REG_LED_RGB, bytes((r, g, b)))

    def rgbs(self, colors):
        """Set each LED to its own (r, g, b). Firmware v1 can't, so it gets
        colors[0] on all of them."""
        if not self._available():
            return
        # v1 firmware doesn't bounds check, so writing reg 18+ trashes its RAM
        if self.version < 2:
            self.rgb(*colors[0])
            return
        self._call(self._write, _REG_LED0, bytes(c for rgb in colors for c in rgb))

    def status_led(self, on):
        """Turn the LED on the back on or off"""
        self._call(self._write, _REG_LED_STATUS, bytes((1 if on else 0,)))

    def position(self):
        """0 for no touch, 1-255 clockwise from the bottom. None if no
        wheel."""
        return self._call(self._read, _REG_POSITION)

    def touches(self):
        """Bitfield of which of the 3 pads are touched. None if no wheel."""
        return self._call(self._read, _REG_TOUCHES)

    def angle(self):
        """Touch angle in degrees, 0-359 clockwise from the bottom. None if
        no touch or no wheel."""
        pos = self.position()
        return pos * 360 // 256 if pos else None

    def delta(self):
        """Signed change in position since the last call, in 1/256ths of a
        turn, positive clockwise. 0 when not touched, for knob-style
        scrolling."""
        pos = self.position()
        last, self._last_pos = self._last_pos, pos or None
        if not pos or last is None:
            return 0
        # Shortest way around, so crossing 255->1 isn't a big jump
        return (pos - last + 128) % 256 - 128


def color_wheel(pos):
    """Rainbow color for pos 0-255"""
    pos &= 0xFF
    if pos < 85:
        return 255 - pos * 3, pos * 3, 0
    if pos < 170:
        pos -= 85
        return 0, 255 - pos * 3, pos * 3
    pos -= 170
    return pos * 3, 0, 255 - pos * 3
