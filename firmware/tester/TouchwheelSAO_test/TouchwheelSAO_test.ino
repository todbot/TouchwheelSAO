// TouchwheelSAO_test.ino -- test TouchwheelSAO in Arduino
// 5 Nov 2024 - @todbot / Tod Kurt
// assumes usuing arduino-pico arduino core
// tune the Wire setup to match your system

#include <Wire.h>

const int i2c_addr = 0x54; 
//const int i2c_addr = 0x56; // tod's test wheel

class TouchwheelSAO {
public:
  TouchwheelSAO() {}

  void begin(TwoWire *aWire = &Wire, uint8_t an_i2c_addr = 0x54) {
    theWire = aWire;
    i2c_addr = an_i2c_addr;
  }

  int readPos() {
    return readReg(0);
  }

  int readReg(uint8_t reg) {
    theWire->beginTransmission(i2c_addr);
    theWire->write(reg);
    theWire->endTransmission(); // false == do not release bus
    //theWire->endTransmission(false); // false == do not release bus
    theWire->requestFrom(i2c_addr,1);
    int v = theWire->read();
    return v;
  }

  // write to a particular register of the chip with a given value
  void writeReg(uint8_t reg, uint8_t regval) {
    theWire->beginTransmission(i2c_addr);
    theWire->write(reg); 
    theWire->write(regval); 
    theWire->endTransmission();
  }
  uint8_t i2c_addr;
  TwoWire *theWire;
};


TouchwheelSAO touchwheel = TouchwheelSAO();

void setup()
{
  Serial.begin(115200);

  // change these to match your I2C setup
  Wire.setSCL(17); // SAO connector on pins 16,17 (which are on i2c0 aka 'Wire')
  Wire.setSDA(16);
  Wire.begin();   // join i2c bus

  touchwheel.begin(&Wire, i2c_addr);

  for( int i=0; i<10; i++) { 
    Serial.println("blink!");
    touchwheel.writeReg(14, 1);  // status LED on
    delay(300);
    touchwheel.writeReg(14, 0);  // status LED off
    delay(300);
  }
  Serial.println("hello from setup");
}


void loop() 
{
  int val = touchwheel.readPos();
  Serial.printf("loop! position:%d\n", val);

  delay(100);

}
