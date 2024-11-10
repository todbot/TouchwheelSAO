// originally from https://github.com/todbot/TouchyTouch
/**
 * TouchyTouch - Simple capacitive sense touch library that mimics how CircuitPython touchio works
 *               But also with debounced pressed() / released() events
 *
 * 7 Mar 2023 - @todbot / Tod Kurt
 *
 * See: https://gist.github.com/todbot/27c34c55d36002c601b2c28ae8f1b8a4
 * more really: https://github.com/adafruit/circuitpython/blob/main/shared-module/touchio/TouchIn.c
 *
 */

#define N_SAMPLES 10         // default is 10 in touchio, 15-20 works better on picotouch?
#define CHARGE_MICROS 10     // default is 10 in touchio
#define TIMEOUT_TICKS 10000
//#if defined(ARDUINO_ARCH_RP2040)
//#define OUTPUT_STYLE OUTPUT_12MA  // RP2040 can select a higher drive, default is OUTPUT_4MA
//#else
#define OUTPUT_STYLE OUTPUT  // for all Arduino
//#endif

class TouchyTouch
{
 public:
  TouchyTouch() {}

  /*!
    @brief Set up a particular touch pin, automatically sets threshold and debounce_interval
     but those can be changed later for tuning
  */
  void begin(int apin = -1, uint16_t debounce_millis=10) {
    pin = apin;
    recalibrate();
    debounce_interval = debounce_millis;
    last_state = false;
    changed = false;
  }

  /*!
     @brief Recalibrate threshold value, called automatically on begin().
     Returns an optional smoothed raw_value to use for hand recalibration
  */
  uint16_t recalibrate() {
    const int num_reads = 5;
    raw_value = 0;
    for(int i=0; i<num_reads; i++) {
      raw_value += rawRead();
    }
    raw_value /= num_reads;
    threshold = (raw_value * 1.05) + 100;
    return raw_value;
  }

  /*!
     @brief Call update() as fast as possible. After, touched()/pressed()/released() are valid.
  */
  void update() {
    changed = false;
    uint32_t now = millis();
    if( now - last_debounce_millis > debounce_interval ) {
      last_debounce_millis = now;
      bool touch_state = readTouch();
      changed = touch_state != last_state;
      last_state = touch_state;
    }
  }

  /*!
     @brief Returns true if pin signal transitions to touched on last update()
  */
  bool pressed() {
    return changed && last_state==true;
  }

  /*!
     @brief Returns true if pin signal transitions to released on last update().
  */
  bool released() {
    return changed && last_state==false;
  }

  /*!
     @brief Returns true if pin signal transitions to touched. same as pressed()
     @note  This function is deprecated, here only for old projects
  */
  bool rose() {
    return changed && last_state==true;
  }

  /*!
     @brief Returns true if pin signal transitions to released. same as released()
     @note  This function is deprecated, here only for old projects
  */
  bool fell() {
    return changed && last_state==true;
  }

  /*!
     @brief Returns true if the button is currently physically pressed.
  */
  inline bool touched() {
    return (raw_value > threshold);
  }

  /*!
     @brief Returns true if the button is currently physically pressed.
     Will cause a read to happen
  */
  bool readTouch() {
    raw_value = rawRead();
    return touched();
  }

  /*!
     @brief Returns a raw_value 0-10000 value, does actual touch detection, called by isTouched().
  */
  int16_t rawRead() {
    uint16_t ticks = 0;
    for (uint16_t i = 0; i < N_SAMPLES; i++) {
      // set pad to digital output high for 10us to charge it
      pinMode(pin, OUTPUT_STYLE);
      digitalWrite(pin, HIGH);
      delayMicroseconds(CHARGE_MICROS);
      // set pad back to an input and take some samples
      noInterrupts();
      pinMode(pin, INPUT);
      while ( digitalRead(pin) ) {
        if (ticks >= TIMEOUT_TICKS) {
          interrupts();
          return TIMEOUT_TICKS;
        }
        ticks++;
      }
      interrupts();
    }
    return ticks;
  }

  uint32_t last_debounce_millis;   ///< for debounce
  uint16_t debounce_interval;   ///< for debounce
  bool last_state;      ///< for debounce
  bool changed;         ///< for debounce
  uint16_t threshold;   ///< the threshold auto-calculated on begin()
  uint16_t raw_value;   ///< raw touch value, compared against threshold
  int pin;              ///< the pin this object is using
};
