/**
 * TouchwheelSAO_attiny816.ino -- 
 * 9 Aug 2024 - @todbot / Tod Kurt
 * 
 * - Designed for ATtiny816
 * - Uses "megaTinyCore" Arduino core: https://github.com/SpenceKonde/megaTinyCore/
 * - Configuration in Tools:
 *   - Defaults
 *   - Clock: 10 MHz internal
 *   - printf: minimal
 *   - Programer: "SerialUPDI - SLOW 57600 baud"
 * - Program with "Upload with Programmer" with a USB-Serial dongle 
 #    as described here: 
 *    https://learn.adafruit.com/adafruit-attiny817-seesaw/advanced-reprogramming-with-updi
 * 
 **/

#include <Wire.h>
#include "TouchyTouch.h"
// #include <tinyNeoPixel_Static.h>  
#include "Adafruit_seesawPeripheral_tinyneopixel.h"
// using seesaw's neopixel driver instead of megatinycore's tinyneopixel 
//  because it saves 500 bytes of flash
// but seems to glitch out, so going back to tinyNeoPixel_Static for now
// update: ahh seems like tinyNeoPixel_Static calls `noInterrupts()` so we'll mirror that

#define MY_I2C_ADDR 0x54
#define LED_BRIGHTNESS  80     // range 0-255
#define LED_UPDATE_MILLIS (2)
#define TOUCH_THRESHOLD_ADJ (1.2)
#define FPOS_FILT (0.05)

// note these pin numbers are the megatinycore numbers: 
// https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/variants/txy6/pins_arduino.h
#define LED_STATUS_PIN  5 // PB4
#define NEOPIXEL_PIN 12  // PC2
#define MySerial Serial
#define NUM_LEDS (3)

enum Register { 
  REG_POSITION = 0,   // angular position 1-255 of touch, or 0 if no touch
  REG_TOUCHES  = 1,   // bitfield of three booleans, one for each touch pad
  REG_RAW0L    = 2,   // touchpad 0 raw count, low byte
  REG_RAW0H    = 3,   // touchpad 0 raw count, high byte
  REG_RAW1L    = 4,
  REG_RAW1H    = 5,
  REG_RAW2L    = 6,
  REG_RAW2H    = 7,
  REG_THRESH0L = 8,   // touchpad 0 threshold, low byte
  REG_THRESH0H = 9,   // touchpad 0 threshold, high byte
  REG_THRESH1L = 10,
  REG_THRESH1H = 11,
  REG_THRESH2L = 12,
  REG_THRESH2H = 13,
  REG_LED_STATUS = 14, // boolean to set status LED
  REG_LED_RGBR = 15,   // LED ring color R
  REG_LED_RGBG = 16,   // LED ring color G
  REG_LED_RGBB = 17,   // LED ring color B 
  REG_NUMREGS  = 18,
  REG_NONE     = 255,
};

const int touch_pins[] = {0, 1, 2, };
const int touch_count = sizeof(touch_pins) / sizeof(int);
TouchyTouch touches[touch_count];

uint32_t last_debug_time;
uint32_t last_led_time;
int pos = 0;
uint8_t touch_timer = 255;
uint8_t held_timer = 0;
bool do_startup_demo = true;

uint8_t regs[REG_NUMREGS]; // the registers to send/receive via i2c
uint8_t curr_reg = REG_POSITION; 

// background LED ring color
#define ledr (regs[REG_LED_RGBR])
#define ledg (regs[REG_LED_RGBG])
#define ledb (regs[REG_LED_RGBB])

volatile uint8_t led_buf[NUM_LEDS*3]; // 3 bytes per LED

// when using tinyNeoPixel_Static
// tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB, (uint8_t*) led_buf);
// #define pixel_set(n,r,g,b) leds.setPixelColor(n, leds.Color(r,g,b))
// #define pixel_fill(r,g,b) leds.fill(leds.Color(r,g,b))
// #define pixel_show() leds.show()

// when using adafruit_seesawperipheral_tinyneopixel
void pixel_set(uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
  led_buf[n*3+0] = (uint16_t)(g * LED_BRIGHTNESS) / 256; // G
  led_buf[n*3+1] = (uint16_t)(r * LED_BRIGHTNESS) / 256; // R 
  led_buf[n*3+2] = (uint16_t)(b * LED_BRIGHTNESS) / 256; // B
}
void pixel_fill(uint8_t r, uint8_t g, uint8_t b) { 
  for(int i=0; i<NUM_LEDS; i++) { 
    pixel_set(i, r,g,b);
  }
}
void pixel_show() {
  noInterrupts();
  tinyNeoPixel_show(NEOPIXEL_PIN, NUM_LEDS*3, (uint8_t *)led_buf);
  interrupts();
}


// originally from https://github.com/todbot/touchwheels/blob/main/arduino/touchwheel0_test0/touchwheel0_test0.ino
// but this is the int, no float, version
//
// return 0-255 touch position on wheel, or -1 if not touched
//
int wheel_pos() {
  // 0-255 "percentage" of touch per pad
  // int a_pct = (((int)touches[0].raw_value - touches[0].threshold) * 255) / touches[0].threshold;
  // int b_pct = (((int)touches[1].raw_value - touches[1].threshold) * 255) / touches[1].threshold;
  // int c_pct = (((int)touches[2].raw_value - touches[2].threshold) * 255) / touches[2].threshold;
  int a_pct = ((touches[0].raw_value - touches[0].threshold) * 255);
  int b_pct = ((touches[1].raw_value - touches[1].threshold) * 255);
  int c_pct = ((touches[2].raw_value - touches[2].threshold) * 255);
  a_pct /= (int)(touches[0].threshold);  // can't get the compiler to keep this signed when one-line
  b_pct /= (int)(touches[1].threshold);  // so we split it up
  c_pct /= (int)(touches[2].threshold); 

  int pos = -1;
  if( a_pct >= 0 and b_pct >= 0 ) { 
    pos = 0*85 + 85 * b_pct / (a_pct + b_pct);
  }
  else if( b_pct >= 0 and c_pct >= 0 ) {
    pos = 1*85 + 85 * c_pct / (b_pct + c_pct);
  }
  else if( c_pct >= 0 and a_pct >= 0 ) { 
    pos = 2*85 + 85 * a_pct / (c_pct + a_pct);
  }
  // special cases when finger is just on a single pad.
  // these shouldn't be needed and create "deadzones" at these points
  // so surely there's a better solution
  else if( a_pct > 0 and b_pct <= 0 and c_pct <= 0 ) {  // just "a"
    pos = 0*85;
  }
  else if( a_pct <= 0 and b_pct > 0 and c_pct <= 0 ) {  // just "b"
    pos = 1*85;
  }
  else if( a_pct <= 0 and b_pct <= 0 and c_pct > 0 ) {  // just "c"
    pos = 2*85;
  }

  return pos;  
}

void startup_demo() {
  // do a little fade up to indicate we're alive
  for(byte i=0; i<100; i++) { 
    ledr = i, ledg = i, ledb = i;
    pixel_fill(ledr, ledg, ledb);
    pixel_show();
    delay(5);
  }
  for(byte i=0; i<255; i++) { 
    for(byte n=0; n<3; n++) { 
      uint32_t c = wheel(i + n*50);
      uint8_t r = (c>>16) & 0xff, g = (c>>8) & 0xff, b = c&0xff;
      //r /= 2; g /= 2; b /= 2;  // dim a bit
      pixel_set(n, r,g,b);
    }
    pixel_show();
    delay(5);
  }
}

void setup() {
  MySerial.begin(115200);
  MySerial.println("\r\nHello I'm an I2C device");
  
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(NEOPIXEL_PIN, OUTPUT);
    
  Wire.onReceive(receiveDataWire);
  Wire.onRequest(transmitDataWire);
  Wire.begin(MY_I2C_ADDR);

  // Touch buttons
  for (int i = 0; i < touch_count; i++) {
    touches[i].begin( touch_pins[i] );
    touches[i].threshold = touches[i].raw_value * TOUCH_THRESHOLD_ADJ; // auto threshold doesn't work
  }
  //touch_recalibrate();
}

void touch_recalibrate() {
    for( int i=0; i< touch_count; i++) { 
      uint16_t v = touches[i].recalibrate();
      touches[i].threshold = v * TOUCH_THRESHOLD_ADJ; // auto threshold doesn't work
    }
}

// main loop
void loop() {
  if(do_startup_demo) { 
    startup_demo();
    // recalibrate in case pads were being touched on power up (or power not stable)
    touch_recalibrate();
    do_startup_demo = false;
  }

  // update touch inputs
  for ( int i = 0; i < touch_count; i++) {
    touches[i].update(); 
  }
  uint8_t touched = touches[0].touched() << 2 | touches[1].touched() << 1 | touches[2].touched();

  // simple iir filtering on touch inputs
  // (not implemented yet)
  pos = wheel_pos();
  if(touched) {
    touch_timer = min(touch_timer+10, 255);

    if( touch_timer > 10 ) { // held
      held_timer = min(held_timer+10, 255);
    }
    if( pos < 1 ) { pos = 1; }  // hack
  }
  else { 
    pos = 0;
  }

  // set the I2C registers
  regs[REG_POSITION] = pos;  // FIXME
  regs[REG_TOUCHES] = touched;
  regs[REG_RAW0L] = touches[0].raw_value;     // high and low byte
  regs[REG_RAW0H] = touches[0].raw_value>>8;  // high and low byte
  regs[REG_RAW1L] = touches[1].raw_value;
  regs[REG_RAW1H] = touches[1].raw_value>>8;
  regs[REG_RAW2L] = touches[2].raw_value;
  regs[REG_RAW2H] = touches[2].raw_value>>8;
  regs[REG_THRESH0L] = touches[0].threshold;     // high and low byte
  regs[REG_THRESH0H] = touches[0].threshold>>8;  // high and low byte
  regs[REG_THRESH1L] = touches[1].threshold;
  regs[REG_THRESH1H] = touches[1].threshold>>8;
  regs[REG_THRESH2L] = touches[2].threshold;
  regs[REG_THRESH2H] = touches[2].threshold>>8;

  // act on I2C output registers
  digitalWrite(LED_STATUS_PIN, regs[REG_LED_STATUS] > 0 ? HIGH : LOW);

  // LED update
  uint32_t now = millis();
  if( now - last_led_time > LED_UPDATE_MILLIS ) { // only update neopixels every N msec
    last_led_time = now;
    // if touched recently, fade down LEDs
    if( touch_timer ) { 
      touch_timer = max(touch_timer-1, 0);
      // fade down background LED
      ledr = max(ledr - 1, 0);
      ledg = max(ledg - 1, 0);
      ledb = max(ledb - 1, 0);
    }
    // only allow I2C setting of RGB LEDs if not recently touched
    else { 
      ledr = regs[REG_LED_RGBR];
      ledg = regs[REG_LED_RGBG];
      ledb = regs[REG_LED_RGBB];
    }

    // set background (or set commanded LEDs)
    pixel_fill(ledr,ledg,ledb);

    // if touched, light up
    if( touched ) { 
      //touch_timer = 255;
      uint32_t c = wheel(pos);
      // sigh, unpack: fixme: change wheel to take r,g,b ptrs
      ledr = (c>>16) & 0xff;
      ledg = (c>>8) & 0xff;
      ledb = (c>>0) & 0xff;
    }
    pixel_show();
  }

  // debug
  if( now - last_debug_time > 50 ) { 
    last_debug_time = now;
    MySerial.printf("pos: %d %d\r\n", pos, touch_timer);
  }

  //  delay(1);

} // loop()

// receive I2C
void receiveDataWire(int16_t numBytes) {      // the Wire API tells us how many bytes
  curr_reg = Wire.read();                  // first byte is address to read from / write to
  MySerial.printf("recv: reg:%02x ", curr_reg);
  if( curr_reg >= REG_NUMREGS ) { 
    MySerial.printf("bad reg addr: %d\r\n", curr_reg);
  }
  else { 
    for (uint8_t i = 0; i < numBytes-1; i++) { 
      uint8_t c = Wire.read();
        MySerial.printf("writing regs[%d] = %02x\r\n", curr_reg, c);
        regs[curr_reg] = c;
        curr_reg++;
    }
  }
} 

// send I2C 
void transmitDataWire() {
  uint8_t c = regs[curr_reg];
  //MySerial.printf("send: %02x\r\n", c);
  Wire.write(c);
  curr_reg++;  // FIXME: test this
}

static uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}
  
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return pack_color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return pack_color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pack_color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

