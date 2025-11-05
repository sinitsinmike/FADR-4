/******************************
   FADR-4 v1.0.0
   for Teensy LC (www.pjrc.com)
   by Notes and Volts
   www.notesandvolts.com
 ******************************/

/**************************************
   ** Upload Settings **
   Board: "Teensy LC"
   USB Type: "MIDI"
   CPU Speed: "48 Mhz"
 **************************************/


/*** Cross‑platform MIDI + EEPROM + pins (Teensy LC / RP2040 / ESP32‑S3)
     - RP2040/ESP32 use TinyUSB + FortySevenEffects MIDI library
     - Standardize ADC to 10-bit for consistent 0..127 scaling
     - Replace usbMIDI.* with MIDI_*() wrappers
***/

#if defined(TEENSYDUINO)
  #define FADR_PLATFORM_TEENSY 1
#else
  #define FADR_PLATFORM_GENERIC 1
#endif

// Safe defaults for MAX7219 pins (override in build flags or before this include)
#ifndef MAX7219_DIN_PIN
  #define MAX7219_DIN_PIN 4
#endif
#ifndef MAX7219_CLK_PIN
  #define MAX7219_CLK_PIN 2
#endif
#ifndef MAX7219_CS_PIN
  #define MAX7219_CS_PIN 3
#endif

#if defined(FADR_PLATFORM_GENERIC)
  #include <Adafruit_TinyUSB.h>
  #include <MIDI.h>
  static Adafruit_USBD_MIDI USBD_MIDI; // USB device MIDI transport
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, USBD_MIDI, MIDI_IF);
  #define MIDI_BEGIN()   do { MIDI_IF.begin(MIDI_CHANNEL_OMNI); } while(0)
  #define MIDI_READ()    MIDI_IF.read()
  #define MIDI_SEND_CC(c,v,ch)  MIDI_IF.sendControlChange((c),(v),(ch))
#else
  #define MIDI_BEGIN()   do {} while(0)
  #define MIDI_READ()    usbMIDI.read()
  #define MIDI_SEND_CC(c,v,ch)  MIDI_SEND_CC((c),(v),(ch))
#endif

// EEPROM compatibility
#if defined(ARDUINO_ARCH_ESP32)
  #include <EEPROM.h>
  #ifndef EEPROM_SIZE
    #define EEPROM_SIZE 1024
  #endif
  inline void EEPROM_BEGIN() { EEPROM.begin(EEPROM_SIZE); }
  inline void EEPROM_COMMIT() { EEPROM.commit(); }
#elif defined(ARDUINO_ARCH_RP2040)
  #include <EEPROM.h>
  inline void EEPROM_BEGIN() {}   // Arduino-Pico emulates, no begin needed
  inline void EEPROM_COMMIT() {}  // no commit needed
#else
  // Teensy and others
  inline void EEPROM_BEGIN() {}
  inline void EEPROM_COMMIT() {}
#endif

// Helper macro to always commit where required
#define EEPROM_WRITE(addr, val) do { EEPROM_WRITE((addr),(val)); EEPROM_COMMIT(); } while(0)

// Use 4 analog inputs: F1,F2,JoyX,JoyY
// Override these if your board uses different analog-capable pins.
#ifndef FADR_A0
  #define FADR_A0 A0
#endif
#ifndef FADR_A1
  #define FADR_A1 A1
#endif
#ifndef FADR_A2
  #define FADR_A2 A2
#endif
#ifndef FADR_A3
  #define FADR_A3 A3
#endif
#include <LedControl.h>
#include <EEPROM.h>

#define EEPROM_KEY 200
#define LED_LEVEL 2

#ifndef EDIT_BUTTON
#define EDIT_BUTTON 12
#endif

LedControl mydisplay = LedControl(MAX7219_DIN_PIN, MAX7219_CLK_PIN, MAX7219_CS_PIN, 1);

byte memStart = 2; // Start of Faders in EEPROM
byte bank = 0;
int oldValue[4];
int pins[] = {FADR_A0, FADR_A1, FADR_A2, FADR_A3};
byte cc[][8] = {
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8},
  {1, 2, 3, 4, 5, 6, 7, 8}
};
byte mChan[] = {1, 2, 3, 4, 5, 6, 7, 8};
byte val = 0;

void setup() {
  #if defined(analogReadResolution)
  analogReadResolution(10);
  #endif
  EEPROM_BEGIN();
  MIDI_BEGIN();
  pinMode(EDIT_BUTTON, INPUT_PULLUP); //Edit Button
  mydisplay.setIntensity(0, LED_LEVEL); // 15 = brightest
  mydisplay.shutdown(0, false);  // turns on display
  mydisplay.setRow(0, 0, 0x76);
  mydisplay.setRow(0, 1, 0x77);
  mydisplay.setRow(0, 2, 0x3e);

  initRom();
  readRom();
  if (digitalRead(EDIT_BUTTON) == LOW) {
    mydisplay.setChar(0, 0, 8, true);
    mydisplay.setChar(0, 1, 8, true);
    mydisplay.setDigit(0, 2, 8, true);
    delay(2000);
    mydisplay.setDigit(0, 0, 1, true); // Version 1.0.0
    mydisplay.setDigit(0, 1, 0, true);
    mydisplay.setDigit(0, 2, 0, false);
    delay(8000);
  }
  if (digitalRead(EDIT_BUTTON) == LOW) {
    showRom();
  }
  delay(2000);
  faderReset();
}

void loop() {
  MIDI_READ();
  switch (checkButton()) {
    case 1:
      displayMode();
      break;
    case 2:
      channelEdit();
      break;
  }
  for (int x = 0; x < 4; x++) {
    val = getFaderValue(x);
    if (val < 255) {
      MIDI_SEND_CC(cc[bank][x], (val), mChan[0]);
      threeDigit(val);
    }
  }
}

void faderReset() {
  for (int x = 0; x < 4; x++) {
    val = getFaderValue(x);
    if (val < 255) {
    }
  }
  mydisplay.setChar(0, 0, '-', false);
  mydisplay.setChar(0, 1, '-', false);
  mydisplay.setChar(0, 2, '-', false);
}

void channelEdit() {
  int channelVal = mChan[0];
  chanDigit(channelVal);

  while (checkButton() != 1) {
    MIDI_READ();
    for (int x = 0; x < 4; x++) {
      val = getFaderValue(x);
      if (val < 255) {
        channelVal = (val >> 3) + 1;
        chanDigit(channelVal);
      }
    }
  }
  mChan[0] = channelVal;
  if (mChan[0] != EEPROM.read(1)) EEPROM_WRITE(1, mChan[0]);
  mydisplay.setIntensity(0, 0);
  delay(100);
  mydisplay.setIntensity(0, LED_LEVEL);
  delay(100);
  mydisplay.setIntensity(0, 0);
  delay(100);
  mydisplay.setIntensity(0, LED_LEVEL);
  delay(100);

  faderReset();
}

void displayMode() {
  unsigned long buttonMillis;
  unsigned long currentMillis;
  unsigned long delayMillis;
  byte faderSelected = 1;
  bool toggle = true;
  byte buttonStat = 0;

  buttonMillis = millis();
  delayMillis = buttonMillis;//TEST

  while (1) {
    MIDI_READ();
    currentMillis = millis();
    buttonStat = checkButton();
    if (buttonStat == 1) {
      faderSelected++;
      if (faderSelected > 4) faderSelected = 1;
      mydisplay.setChar(0, 0, 'F', false);
      mydisplay.setDigit(0, 1, faderSelected, false);
      mydisplay.setChar(0, 2, ' ', false);
      buttonMillis = millis();
      delayMillis = buttonMillis;
      currentMillis = buttonMillis;
      toggle = false;
    }

    else if (buttonStat == 2) {
      threeDigit(cc[bank][faderSelected - 1]);
      faderEdit(faderSelected);
      buttonMillis = millis();
      delayMillis = buttonMillis;
      currentMillis = buttonMillis;
      toggle = false;
    }

    if (currentMillis - delayMillis >= 1000) {
      if (toggle == true) {
        mydisplay.setChar(0, 0, 'F', false);
        mydisplay.setDigit(0, 1, faderSelected, false);
        mydisplay.setChar(0, 2, ' ', false);
      }
      else if (toggle == false) {
        threeDigit(cc[bank][faderSelected - 1]);
      }
      toggle = !toggle;
      delayMillis = millis();
    }

    if (currentMillis - buttonMillis > 8000) {
      faderReset();
      return;
    }
  }
}

void faderEdit(byte fader) {
  unsigned long delayMillis;
  bool toggle = true;
  byte temp = cc[0][fader - 1];

  delayMillis = millis();
  while (checkButton() == 2) {
    MIDI_READ();
    if (millis() - delayMillis >= 100) {
      if (toggle == true) mydisplay.setIntensity(0, 0);
      else mydisplay.setIntensity(0, LED_LEVEL);
      delayMillis = millis();
      toggle = !toggle;
    }
    for (int x = 0; x < 4; x++) {
      val = getFaderValue(x);
      if (val < 255) {
        threeDigit(val);
        temp = val;
        cc[0][fader - 1] = val;
      }
    }
  }
  if (temp != EEPROM.read((fader - 1) + memStart)) EEPROM_WRITE((fader - 1) + memStart, temp); //Write new CC to EEProm
  mydisplay.setIntensity(0, LED_LEVEL);
  return;
}

void threeDigit(int number) {
  int first = number / 100;
  int secon = number % 100 / 10;
  int third = number % 10;
  mydisplay.setDigit(0, 2, third, false);
  mydisplay.setDigit(0, 1, secon, false);
  mydisplay.setDigit(0, 0, first, false);
}

void chanDigit(int number) {
  int secon = number % 100 / 10;
  int third = number % 10;
  mydisplay.setDigit(0, 2, third, false);
  mydisplay.setDigit(0, 1, secon, false);
  mydisplay.setChar(0, 0, 'C', false);

}

int getFaderValue(int pin) {
  int value = analogRead(pins[pin]);
  int tmp = (oldValue[pin] - value);
  if (tmp >= 8 || tmp <= -8) {
    value = analogRead(pins[pin]);
    tmp = (oldValue[pin] - value);
    if ((tmp >= 8) || (tmp <= -8)) {
      if (value == 8) oldValue[pin] = value + 1;// Zero Fix
      else oldValue[pin] = value;
      if (value < 8) return 0; //test
      else return (value >> 3); //test
    }
  }
  return 255;
}

void editBank() {
  bank++;
  if (bank > 7) bank = 0;
  mydisplay.clearDisplay(0);
  mydisplay.setChar(0, 0, 'B', false);
  mydisplay.setChar(0, 2, (bank + 1), false);
  delay(500);
}

void readRom() {
  mChan[0] = EEPROM.read(1); //Read Midi Channel

  for (int bank = 0; bank < 8; bank++) {
    for (int fader = 0; fader < 8; fader++) {
      cc[bank][fader] = EEPROM.read((bank * 8) + (fader + memStart));
    }
  }
}

void initRom() {
  byte key = 102; // First CC Number

  if (EEPROM.read(0) != EEPROM_KEY) {
    EEPROM_WRITE(0, EEPROM_KEY); // Key

    EEPROM_WRITE(1, 1); // Set Midi channel to 1

    for (int bank = 0; bank < 8; bank++) {
      for (int fader = 0; fader < 8; fader++) {
        EEPROM_WRITE((bank * 8) + (fader + memStart), key);
        key++;
      }
    }
    mydisplay.setChar(0, 0, ' ', true);
    mydisplay.setChar(0, 1, ' ', true);
    mydisplay.setChar(0, 2, ' ', true);
    delay(1000);
  }
}

byte checkButton() {
  const int debounce = 50;
  const int longPress = 2000;
  static byte buttonState = 0;
  static unsigned long buttonMillis;
  static unsigned long currentMillis;

  switch (buttonState) {
    case 0: // Nothing
      if (digitalRead(EDIT_BUTTON) == LOW) {
        buttonState = 1;
        buttonMillis = millis();
      }
      return 0;
      break;
    case 1: // Test
      currentMillis = millis();
      if (currentMillis - buttonMillis > debounce) {
        if (digitalRead(EDIT_BUTTON) == LOW) buttonState = 2;
        else buttonState = 0;
      }
      return 0;
      break;
    case 2: // Valid
      currentMillis = millis();
      if (digitalRead(EDIT_BUTTON) == LOW) {
        if (currentMillis - buttonMillis > longPress) {
          buttonState = 3;
          return 2;
        }
      }
      else {
        buttonState = 0;
        return 1;
      }
      return 0;
      break;
    case 3: // Long
      if (digitalRead(EDIT_BUTTON) == HIGH) {
        buttonState = 0;
        return 0;
      }
      else return 2;
      break;
    default:
      buttonState = 0;
      return 0;
      break;
  }
}

void showRom() {
  for (int i = 0; i < 6; i++) {
    threeDigit(EEPROM.read(i));
    delay(2000);
  }
}
