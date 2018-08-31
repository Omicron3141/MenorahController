/* MenorahControl for Teensyduino
   Written by Jonah Rubin and Ken Rubin
*/

#include "SPI.h" // necessary libraries
#include "Time.h" 
#include <TimeLib.h> 

const int ledPins = 9; // A Menorah as 9 candles (1 for each of the 8 nights and the shamash)
int ledPin[ledPins] = {20,3,4,9,16,23,22,6,17}; //The pins in the array start with the shamash and then each pin represents a subsequent candle

//int testledPin = 13;

int selectbuttonPin = 18; // select button is used to sequence through days and through hours
int modebuttonPin = 7; // mode button is used to change into day mode, hour mode, and run mode

unsigned long selectbuttonLastPress = 0; // holds the last time (in milliseconds) that the select button was pressed
unsigned long modebuttonLastPress = 0; // holds the last time (in milliseconds) that the mode button was pressed

// delay between accepted button presses in milliseconds
// the delay is necessary to ensure that one push of the button won't cause multiple changes
int validPressDelay = 300;

int ssPin = 10;

// 0 = day select, 1 = hour select, 2 = run mode
const int daySelect = 0; // this is the mode for selecting the day
const int hourSelect = 1; // this is the mode for selecting the hour in 24 hr military format
const int systemRun = 2; // this is the normal run mode used once the day and hour have been set

int mode = daySelect; // when the Teensy boots up, it will come up in day select mode

// assume it is the day before the first night on startup. So if Hannukah starts this evening at sundown, then currentDay would be 0 indicating that this evening will be day one
int currentDay = 0;

// string buffer to hold the characters to be displayed on the 8 segment LED display
char tempDisplayString[10]; 

// keeps track of whether we've incremented up the day within the past hour
// we need to ensure that we don't continuous increment the day more than once when we go from 4:59pm to 5:00pm
bool dayTickoverHappened = false;

void setup() {
  // assume it's noon
  setTime(11, 0, 0, 0, 0, 0);
  
  for (int thisPin = 0; thisPin < ledPins; thisPin++) {  
    pinMode(thisPin, OUTPUT);
  }
  
  pinMode(modebuttonPin, INPUT); // define the Teensy pin used for mode changes to be an input pin
  pinMode(selectbuttonPin, INPUT); // define the Teensy pin used for selection changes to be an input pin

  pinMode(ssPin, OUTPUT); // define the Teensy pin ss to be an output pin
  digitalWrite(ssPin, HIGH);  // Set the SS pin HIGH
  SPI.begin();  // Begin SPI hardware
  SPI.setClockDivider(SPI_CLOCK_DIV64);  // Slow down SPI clock
  clearDisplaySPI();
}

// this function provides the default candle flicker behavior
void flicker(int pin) {
  analogWrite(pin, random(100, 255)); // the default candle flicker is achieved by writing a randon voltage between 100 and 255 to the proper candle pin
}

// this function turns off a candle
void dark(int pin) {
  analogWrite(pin, 0);
}

//the main loop
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
