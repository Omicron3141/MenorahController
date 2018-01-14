#include <TimeLib.h>
#include "SPI.h" // necessary library
#include "Time.h"

const int ledPins = 9;
int ledPin[ledPins] = {3,4,6,9,16,17,20,22,23};

//int testledPin = 13;

int selectbuttonPin = 18;
int modebuttonPin = 7;

unsigned long selectbuttonLastPress = 0;
unsigned long modebuttonLastPress = 0;

// delay between accepted button presses in milliseconds
int validPressDelay = 300;

int ssPin = 10;

// 0 = run, 1 = day select, 2 = hour select
const int daySelect = 0;
const int hourSelect = 1;
const int systemRun = 2;

int mode = daySelect;

// assume it is the day before the first night on startup
int currentDay = 0;

char tempDisplayString[10];

// keeps track of whether we've ticked up the day within the past hour
bool dayTickoverHappened = false;

void setup() {
  // assume it's noon
  setTime(11, 0, 0, 0, 0, 0);
  
  for (int thisPin = 0; thisPin < ledPins; thisPin++) {  
    pinMode(thisPin, OUTPUT);
  }
  
  pinMode(modebuttonPin, INPUT);
  pinMode(selectbuttonPin, INPUT);

  pinMode(ssPin, OUTPUT); // we use this for SS pin
  digitalWrite(ssPin, HIGH);  // Set the SS pin HIGH
  SPI.begin();  // Begin SPI hardware
  SPI.setClockDivider(SPI_CLOCK_DIV64);  // Slow down SPI clock
  clearDisplaySPI();
}

void flicker(int pin) {
  analogWrite(pin, random(100, 255));
}

void dark(int pin) {
  analogWrite(pin, 0);
}
void loop() {
  int illuminatedPins = currentDay;
  if (illuminatedPins <= 0) {
    illuminatedPins = 8;
  }
  for (int thisPin = 0; thisPin <= illuminatedPins; thisPin++) {  
    flicker(ledPin[thisPin]);
  }
  for (int thisPin = illuminatedPins+1; thisPin < ledPins; thisPin++) {  
    dark(ledPin[thisPin]);
  }
  
  clearDisplaySPI();

  
  // handle mode button
  if (readModeButton()) {
    mode++;
    mode = mode % 3;
  } 
  
  // DAY SELECT MODE
  if (mode == daySelect) {
    if (currentDay < 0) {
      sprintf(tempDisplayString, " d-%1d", -1*currentDay);
    } else {
      sprintf(tempDisplayString, " d %1d", currentDay);
    }
    writeDisplaySPI(tempDisplayString);
    setDecimalsSPI(0b00010000);
    if (readSelectButton()) {
      currentDay++;
      if (currentDay > 8) {
        // currently only letting us set up 5 days prior.
        currentDay = -5;
      }
    }
  }

  // HOUR SELECT MODE
  if (mode == hourSelect) {
    sprintf(tempDisplayString, "%4d", (hour()+1) * 100);
    writeDisplaySPI(tempDisplayString);
    setDecimalsSPI(0b00010000);
    if (readSelectButton()) {
      if (hour() == 23) {
        setTime(0, 0, 0, 0, 0, 0);
      } else {
        setTime(hour() + 1, 0, 0, 0, 0, 0);
      }
    }
  }

  // NORMAL OPERATION
  if (mode == systemRun) {
    writeDisplaySPI("run ");
    setDecimalsSPI(0b00000000);
    if (hour() == 16 && !dayTickoverHappened && (currentDay < 8)) {
      currentDay++;
      dayTickoverHappened = true;
    } else if (hour() == 17 && dayTickoverHappened) {
      dayTickoverHappened = false;
    }
  }

  delay(50);
}


bool readSelectButton() {
  if (digitalRead(selectbuttonPin) == HIGH) {
    unsigned long currenttime = millis();
    if ((currenttime-selectbuttonLastPress) > validPressDelay) {
      selectbuttonLastPress = currenttime;
      return true;
    }
  }
  return false;
}


bool readModeButton() {
  if (digitalRead(modebuttonPin) == HIGH) {
    unsigned long currenttime = millis();
    if ((currenttime-modebuttonLastPress) > validPressDelay) {
      modebuttonLastPress = currenttime;
      return true;
    }
  }
  return false;
}


// This custom function works somewhat like a serial.print.
//  You can send it an array of chars (string) and it'll print
//  the first 4 characters in the array.
void writeDisplaySPI(String toSend)
{
  digitalWrite(ssPin, LOW);
  for (int i=0; i<4; i++)
  {
    SPI.transfer(toSend[i]);
  }
  digitalWrite(ssPin, HIGH);
}

// Send the clear display command (0x76)
//  This will clear the display and reset the cursor
void clearDisplaySPI()
{
  digitalWrite(ssPin, LOW);
  SPI.transfer(0x76);  // Clear display command
  digitalWrite(ssPin, HIGH);
}

// Set the displays brightness. Should receive byte with the value
//  to set the brightness to
//  dimmest------------->brightest
//     0--------127--------255
void setBrightnessSPI(byte value)
{
  digitalWrite(ssPin, LOW);
  SPI.transfer(0x7A);  // Set brightness command byte
  SPI.transfer(value);  // brightness data byte
  digitalWrite(ssPin, HIGH);
}

// Turn on any, none, or all of the decimals.
//  The six lowest bits in the decimals parameter sets a decimal 
//  (or colon, or apostrophe) on or off. A 1 indicates on, 0 off.
//  [MSB] (X)(X)(Apos)(Colon)(Digit 4)(Digit 3)(Digit2)(Digit1)
void setDecimalsSPI(byte decimals)
{
  digitalWrite(ssPin, LOW);
  SPI.transfer(0x77);
  SPI.transfer(decimals);
  digitalWrite(ssPin, HIGH);
}
