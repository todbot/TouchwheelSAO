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
#include <tinyNeoPixel_Static.h>
// #include "Adafruit_seesawPeripheral_tinyneopixel.h"
// using seesaw's neopixel driver instead of megatinycore's tinyneopixel 
//  because it saves 500 bytes of flash
// but seems to glitch out, so going back to tinyNeoPixel_Static for now

#define MY_I2C_ADDR 0x54
// note these pin numbers are the megatinycore numbers: 
// https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/variants/txy6/pins_arduino.h
#define LED_STATUS_PIN  5 // PB4, pin 5 on seesaw board
#define NEOPIXEL_PIN 12  // PC2, pin 12 on seesaw board
#define MySerial Serial
#define NUM_LEDS 3

enum Register { 
  REG_POSITION = 0, 
  REG_TOUCHES  = 1,
  REG_RAW0L    = 2,
  REG_RAW0H    = 3,
  REG_RAW1L    = 4,
  REG_RAW1H    = 5,
  REG_RAW2L    = 6,
  REG_RAW2H    = 7,
  REG_STATUS_LED = 8,
  REG_THRESH0L = 9,
  REG_THRESH0H = 10,
  REG_THRESH1L = 11,
  REG_THRESH1H = 12,
  REG_THRESH2L = 13,
  REG_THRESH3H = 14,
  // REG_RAW0     = 2,  // start of all raw values
  // REG_RAW1     = 4,  
  // REG_RAW2     = 6,  
  REG_MAXREG   = 9,
  REG_NONE     = 255,
};

const int touch_pins[] = {0, 1, 2, };
const int touch_count = sizeof(touch_pins) / sizeof(int);
TouchyTouch touches[touch_count];

volatile uint8_t neopixel_buf[NUM_LEDS*3]; // 3 bytes per LED
tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB, (uint8_t*) neopixel_buf);

uint8_t regs[16]; // the registers to send/receive via i2c
uint8_t curr_reg = REG_POSITION; 

#define pixel_set(n,r,g,b) leds.setPixelColor(n, leds.Color(r,g,b))
#define pixel_fill(r,g,b) leds.fill(leds.Color(r,g,b))
#define pixel_show() leds.show()

// void pixel_set(uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
//   neopixel_buf[n*3+0] = g; // G
//   neopixel_buf[n*3+1] = r; // R 
//   neopixel_buf[n*3+2] = b; // B
// }

// void pixel_fill(uint8_t r, uint8_t g, uint8_t b) { 
//   for(int i=0; i<NUM_LEDS; i++) { 
//     pixel_set(i, r,g,b);
//   }
// }

// void pixel_show() {
//     tinyNeoPixel_show(NEOPIXEL_PIN, NUM_LEDS*3, (uint8_t *)neopixel_buf);
// }


float wheel_pos(float offset=0) { 
  // compute raw percentages
  float a_pct = ((float)touches[0].raw_value - touches[0].threshold) / touches[0].threshold;
  float b_pct = ((float)touches[1].raw_value - touches[1].threshold) / touches[1].threshold;
  float c_pct = ((float)touches[2].raw_value - touches[2].threshold) / touches[2].threshold;

  //float offset = -0.333/2;  // physical design is rotated 1/2 a sector anti-clockwise
  //float offset = 0; // -0.333/2;  // physical design is rotated 1/2 a sector anti-clockwise

  float pos = -1;
  //cases when finger is touching two pads
  if( a_pct >= 0 and b_pct >= 0 ) {
      pos = 0 + 0.333 * (b_pct / (a_pct + b_pct));
  } 
  else if( b_pct >= 0 and c_pct >= 0 ) {
      pos = 0.333 + 0.333 * (c_pct / (b_pct + c_pct));
  }
  else if( c_pct >= 0 and a_pct >= 0 ) { 
      pos = 0.666 + 0.333 * (a_pct / (c_pct + a_pct));
  }
  // special cases when finger is just on a single pad.
  // these shouldn't be needed and create "deadzones" at these points
  // so surely there's a better solution
  else if( a_pct > 0 and b_pct <= 0 and c_pct <= 0 ) { 
      pos = 0;
  }
  else if( a_pct <= 0 and b_pct > 0 and c_pct <= 0 ) { 
      pos = 0.333;
  }
  else if( a_pct <= 0 and b_pct <= 0 and c_pct > 0 ) {
      pos = 0.666;
  }
  if( pos == -1 ) { // no touch 
    return -1;
  }
  // wrap pos around the 0-1 circle if offset puts it outside that range
  return fmod(pos + offset, 1);

}

void receiveDataWire(int16_t numBytes) {      // the Wire API tells us how many bytes
  curr_reg = Wire.read();                  // first byte is address to read from / write to
  MySerial.printf("recv: reg:%02x ", curr_reg);
  if( curr_reg >= REG_MAXREG ) { 
    MySerial.printf("bad reg addr: %d\r\n", curr_reg);
  }
  for (uint8_t i = 0; i < numBytes-1; i++) { 
    uint8_t c = Wire.read();
      MySerial.printf("writing regs[%d] = %02x\r\n", curr_reg, c);
      regs[curr_reg] = c;
      curr_reg++;
  }
} 

void transmitDataWire() {
  uint8_t c = regs[curr_reg];
  //MySerial.printf("send: %02x\r\n", c);
  Wire.write(c);
  curr_reg++;  // FIXME: test this
}


void setup() {
  MySerial.begin(115200);
  MySerial.println("\r\nHello I'm an I2C device");

  pinMode(LED_STATUS_PIN, OUTPUT);
  
  pinMode(NEOPIXEL_PIN, OUTPUT);
  // leds.setPixelColor(0, leds.Color(0, 150, 0)); // Moderately bright green color.
  // leds.show(); // This sends the updated pixel color to the hardware.  
  pixel_set(0, 0x33, 0x00, 0x33);
  pixel_show();
      
 // Touch buttons
  for (int i = 0; i < touch_count; i++) {
    touches[i].begin( touch_pins[i] );
    touches[i].threshold = touches[i].raw_value * 1.1;
  }
  
  Wire.onReceive(receiveDataWire);
  Wire.onRequest(transmitDataWire);
  Wire.begin(MY_I2C_ADDR);
}

uint32_t last_debug_time;
uint32_t last_led_time;
uint8_t pos = 0;

void loop() {
  for ( int i = 0; i < touch_count; i++) {
    touches[i].update(); 
  }

  // simple iir filtering 
  float fpos = wheel_pos();
  const float filt = 0.05;
  if(fpos > -1) { 
    fpos = (fpos * 255);
    pos = filt * fpos + (1-filt) * pos;
  }
  else { 
    pos = 0;
  }

  // set the registers
  regs[REG_POSITION] = pos;  // FIXME
  regs[REG_TOUCHES] = touches[0].touched() << 2 | touches[1].touched() << 1 | touches[2].touched();
  regs[REG_RAW0L] = touches[0].raw_value;  // high and low byte
  regs[REG_RAW0H] = touches[0].raw_value>>8;  // high and low byte
  regs[REG_RAW1L] = touches[1].raw_value;  // high and low byte
  regs[REG_RAW1H] = touches[1].raw_value>>8;  // high and low byte
  regs[REG_RAW2L] = touches[2].raw_value;  // high and low byte
  regs[REG_RAW2H] = touches[2].raw_value>>8;  // high and low byte

  // act on output registers
  digitalWrite(LED_STATUS_PIN, regs[REG_STATUS_LED] > 0 ? HIGH : LOW);

  uint32_t now = millis();
  if( now - last_led_time > 1 ) { // only update neopixels every 1 ms
    last_led_time = now;
    pixel_fill(0,0,0);
    if( pos < 85 ) {
      pixel_set(0, pos, 0, 0);
    }
    else if( pos < 170 ) {
      pixel_set(1, pos-85, 0, 0);
    }
    else {
      pixel_set(2, pos-170, 0, 0);
    }

    pixel_show();
  }

  // debug
  if( now - last_debug_time > 50 ) { 
    last_debug_time = now;
    MySerial.printf("pos: %d\r\n", pos);
  }
}

