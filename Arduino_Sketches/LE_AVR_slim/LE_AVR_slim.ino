/* Slim Logic Engine AVR Sketch : 2017-12-02
 * ========================================= 
 * 
 *  This sketch is designed to run on the original Logic Engine AVR board (Atmega328P),
 *  and run both front and rear logics displays. To use the adjustment trimpots you'll
 *  need to place a shunt/jumper on a specific pin (see below).
 * 
 *  Header labeled OUT (D6) = rear logic
 *  Header labeled S2 = front logic
 * 
 *  To adjust front colors, power on, place jumper on S4, make adjustments, remove jumper
 *  To adjust front colors, power on, place jumper on S3, make adjustments, remove jumper
 *  While in an adjustment mode, connect S5 to GND to toggle palette number
 *  To reset to defaults, power off, place jumper on S4, power up
 *  
 * */

#define DEBUG 0 //0=off, 1=on

#define PCBVERSION 2 //what kind of LED PCBs are they? 0 = Originals (with Naboo logo on backs of Front and Rear Logic)
//                                1 = 2014 Version (with Kenny & McQuarry art on Rear, C3PO on Fronts)
//                                2 = 2016 Version (with Deathstar plans on back of Rear Logic)

#define MAX_BRIGHTNESS 175  //can go up to 255, but why? this limit keeps current and heat down, and not noticeably dimmer than 255
#define MIN_BRIGHTNESS 1   //minimum brightness for standard logic patterns that adjustment pots can go down to

#include <FastLED.h>
#include <avr/pgmspace.h> //to save SRAM, some constants get stored in flash memory
#include "EEPROM.h"       //used to store user settings in EEPROM (settings will persist on reboots)

//a struct that holds the current color number and pause value of each LED (when pause value hits 0, the color number gets changed)
//to save SRAM, we don't store the "direction" anymore, instead we pretend that instead of having 16 colors, we've got 31 (16 that cross-fade up, and 15 "bizarro" colors that cross-fade back in reverse)
struct LEDstat {
  byte colorNum;
  byte colorPause;
};

//adjustable settings
struct userSettings {
  byte maxBri;
  byte frontFade;
  byte frontDelay;
  byte frontHue;
  byte rearFade;
  byte rearDelay;
  byte rearHue;
  byte frontPalNum;
  byte rearPalNum;
  byte frontBri;
  byte rearBri;
};
//default settings (will be overwritten from stored EEPROM data during setup(), or stored in EEPROM if current EEPROM values look invalid)
//to-do: add a version number
#define DFLT_FRONT_FADE 2
#define DFLT_FRONT_DELAY 60
#define DFLT_FRONT_HUE 0
#define DFLT_REAR_FADE 3
#define DFLT_REAR_DELAY 200
#define DFLT_REAR_HUE 0
#define DFLT_FRONT_PAL 0
#define DFLT_REAR_PAL 1
#define DFLT_FRONT_BRI 200
#define DFLT_REAR_BRI 200
userSettings settings[] = { MAX_BRIGHTNESS, DFLT_FRONT_FADE, DFLT_FRONT_DELAY, DFLT_FRONT_HUE,
                            DFLT_REAR_FADE, DFLT_REAR_DELAY, DFLT_REAR_HUE,
                            DFLT_FRONT_PAL, DFLT_REAR_PAL, DFLT_FRONT_BRI, DFLT_REAR_BRI
                          };

byte adjMode = 0; // 0 for no adjustments, 1 for front, 3 for rear. if adjMode>0, then trimpots will be enabled
byte prevAdjMode = 0;
byte prevBrightness = 0;
byte prevPalNum = 0;

//some LED and pattern related stuff...
#define TWEENS 8 //lower=faster higher=smoother color crossfades, closely related to the Fade setting
#define FRONT_PIN 2
#define REAR_PIN 6
//#define STATUSLED_PIN 10 //status LED is connected to pin 10, incorrectly labelled 9 on the PCB!!
#define delayPin A0 //analog pin to read keyPause value
#define fadePin A1 //analog pin to read tweenPause value
#define briPin A2 //analog pin to read Brightness value
#define huePin A3 //analog pin to read Color/Hue shift value     
#define FJUMP_PIN 4  //front ADJ pin
#define RJUMP_PIN 3  //rear ADJ pin  
#define PAL_PIN 5  //pin used to switch palettes in ADJ mode 
LEDstat ledStatus[176]; //status array will cover both front and rear logics on a Teensy
CRGB frontLEDs[80];
CRGB rearLEDs[96];
#define TOTALCOLORS (4+(TWEENS*(3)))
#define TOTALCOLORSWBIZ ((TOTALCOLORS*2)-2)
byte allColors[2][TOTALCOLORS][3]; //the allColor array will comprise of two color palettes on a Teensy

#if PCBVERSION<2
#define LED_TYPE WS2812B
#else
#define LED_TYPE SK6812
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRONT LED MAPPING...
#if (PCBVERSION==0)
//mapping for FLD boards from first run (with 48 LEDs per PCB)
const byte frontLEDmap[]PROGMEM = {
  15, 14, 13, 12, 11, 10, 9, 8,
  16, 17, 18, 19, 20, 21, 22, 23,
  31, 30, 29, 28, 27, 26, 25, 24,
  32, 33, 34, 35, 36, 37, 38, 39,
  47, 46, 45, 44, 43, 42, 41, 40, //
  88, 89, 90, 91, 92, 93, 94, 95, //
  87, 86, 85, 84, 83, 82, 81, 80,
  72, 73, 74, 75, 76, 77, 78, 79,
  71, 70, 69, 68, 67, 66, 65, 64,
  56, 57, 58, 59, 60, 61, 62, 63
};
#else
//mapping for newer FLD PCBs (40 LEDs per PCB, lower FLD upside-down)...
const byte frontLEDmap[]PROGMEM = {
  0, 1, 2, 3, 4, 5, 6, 7,
  15, 14, 13, 12, 11, 10, 9, 8,
  16, 17, 18, 19, 20, 21, 22, 23,
  31, 30, 29, 28, 27, 26, 25, 24,
  32, 33, 34, 35, 36, 37, 38, 39,
  79, 78, 77, 76, 75, 74, 73, 72,
  64, 65, 66, 67, 68, 69, 70, 71,
  63, 62, 61, 60, 59, 58, 57, 56,
  48, 49, 50, 51, 52, 53, 54, 55,
  47, 46, 45, 44, 43, 42, 41, 40
};
#endif
//REAR LED MAPPING...
#if (PCBVERSION==0)
//mapping for first RLD (two PCBs soldered together)
const byte rearLEDmap[]PROGMEM = {
  0, 1, 2, 3, 4, 5, 6, 7,  48, 49, 50, 51, 52, 53, 54, 55,
  15, 14, 13, 12, 11, 10, 9, 8,  63, 62, 61, 60, 59, 58, 57, 56,
  16, 17, 18, 19, 20, 21, 22, 23,  64, 65, 66, 67, 68, 69, 70, 71,
  31, 30, 29, 28, 27, 26, 25, 24,  79, 78, 77, 76, 75, 74, 73, 72,
  32, 33, 34, 35, 36, 37, 38, 39,  80, 81, 82, 83, 84, 85, 86, 87,
  47, 46, 45, 44, 43, 42, 41, 40,  95, 94, 93, 92, 91, 90, 89, 88
};
#elif (PCBVERSION==1)
//mapping for single RLD PCB (second parts run on)...
const byte rearLEDmap[]PROGMEM = {
  0, 1, 2, 28, 3, 27, 4, 26, 5, 25, 6, 7, 8, 9, 22, 10, 21, 11, 20, 12, 19, 13, 14, 15,
  31, 30, 29, 32, 33, 34, 35, 36, 37, 38, 39, 24, 23, 40, 41, 42, 43, 44, 45, 46, 47, 18, 17, 16,
  64, 65, 66, 63, 62, 61, 60, 59, 58, 57, 56, 71, 72, 55, 54, 53, 52, 51, 50, 49, 48, 77, 78, 79,
  95, 94, 93, 67, 92, 68, 91, 69, 90, 70, 89, 88, 87, 86, 73, 85, 74, 84, 75, 83, 76, 82, 81, 80
};
#else
//mapping for 2016 RLD (Death Star on back, direct mapping)...
const byte rearLEDmap[]PROGMEM = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
  95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72
};
#endif
//////////////////////////////////////////////////////////////////////////////////////////////////////


//we define both all palettes, in case the user wants to try out rear colors on the front etc
// note that these are not RGB colors, they're HSV
// for help calculating HSV color values see http://joymonkey.com/logic/
#define PAL_COUNT 3
const byte keyColors[PAL_COUNT][4][3]PROGMEM = { { {170, 255, 0} , {170, 255, 136} , {170, 255, 255} , {170, 0, 255}  } , //front colors
  { {87, 206, 0}    , {87, 206, 105}  , {45, 255, 184}  , {0, 255, 250}  } , //rear colors (hues: 87=bright green, 79=yellow green, 45=orangey yellow, 0=red)
  { {0, 255, 0}   , {0, 255, 85}    , {0, 255, 170}   , {0, 255, 255}  }  //monotone (black to solid red)
}; 

//some variables used for status display and timekeeping...
unsigned long currentMillis = millis();
unsigned long statusMillis = millis(); //last time status LED was changed, or effect sequence was started
unsigned long adjMillis = millis(); //last time we entered an adj mode
#define statusDelayR 400
#define statusDelayF 100
unsigned int statusDelay;
bool flipflop;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//function to calculate all colors based on the chosen colorPalNum
//this will get called during setup(), and also if we've remotely commanded to change color palettes
//NEW: added a "bright" parameter, to allow front and rear brightness to be adjusted independantly
void calculateAllColors(byte colorPalNum, bool frontRear = 0, byte brightVal = MAX_BRIGHTNESS) {
  //take a set of 4 key colors from keyColors[3][4][3] and generate 16 colors in allColors[frontRear]
  // 328P will only have one set of full colors, Teensy will have two
  for (byte kcol = 0; kcol < (4); kcol++) { //go through each Key color
    byte workColor[3] = { pgm_read_byte(&keyColors[colorPalNum][kcol][0]) , pgm_read_byte(&keyColors[colorPalNum][kcol][1]) , pgm_read_byte(&keyColors[colorPalNum][kcol][2])  };
    allColors[frontRear][kcol + (TWEENS * kcol)][0] = workColor[0];
    allColors[frontRear][kcol + (TWEENS * kcol)][1] = workColor[1];
    allColors[frontRear][kcol + (TWEENS * kcol)][2] = map8(workColor[2], MIN_BRIGHTNESS, brightVal); //Value (V) is adjusted down to whatever brightness setting we've specified
    if (kcol + (TWEENS * kcol) + 1 != TOTALCOLORS) {
      for (byte el = 0; el < 3; el++) { //loop through H, S and V from this key to the next
        int perStep = int(pgm_read_byte(&keyColors[colorPalNum][kcol + 1][el]) - workColor[el]) / (TWEENS + 1);
        if (perStep != 0) {
          for (byte tweenCount = 1; tweenCount <= TWEENS; tweenCount++) {
            byte val = pgm_read_byte(&keyColors[colorPalNum][kcol][el]) + (tweenCount * perStep);
            if (el == 2) allColors[frontRear][kcol + (TWEENS * kcol) + tweenCount][el] = map8(val, MIN_BRIGHTNESS, brightVal);
            else allColors[frontRear][kcol + (TWEENS * kcol) + tweenCount][el] = val;
          }
        }
        else {
          //tweens for this element (h,s or v) don't change between this key and the next, fill em up
          for (byte tweenCount = 1; tweenCount <= TWEENS; tweenCount++) {
            /*if (el==2) {
              //allColors[frontRear][kcol+(TWEENS*kcol)+tweenCount][el]=map(workColor[el], 0, 255, 0, bright);
              allColors[frontRear][kcol+(TWEENS*kcol)+tweenCount][el]=map(workColor[el], 0, 255, 0, bright);  //Value (V) is adjusted down to whatever brightness setting we've specified
              }
              else allColors[frontRear][kcol+(TWEENS*kcol)+tweenCount][el]=workColor[el];*/
            if (el == 2) allColors[frontRear][kcol + (TWEENS * kcol) + tweenCount][el] = map8(workColor[el], MIN_BRIGHTNESS, brightVal); //map V depending on brightness setting
            else allColors[frontRear][kcol + (TWEENS * kcol) + tweenCount][el] = workColor[el];
          }
        }
      }
    }
  }
}

//function takes a color number (that may be bizarro) and returns an actual color number
int actualColorNum(int x) {
  if (x >= TOTALCOLORS) x = (TOTALCOLORS - 2) - (x - TOTALCOLORS);
  return (x);
}

void updateLED(byte LEDnum , byte hueVal, bool frontRear, byte briVal = 255) {
  //frontRear is 0 for front, 1 for rear
  //briVal is optional, typically used to change individual LED brightness during effect sequences
  //remember that ledStatus[] has 176 LEDs to track, frontLEDs is an 80 LED object, rearLEDs is a 96 LED object
  //current front pallete is settings[0].frontPalNum
  //current rear pallete is settings[0].rearPalNum
  if (frontRear == 0) {
    if (ledStatus[LEDnum].colorPause != 0)  ledStatus[LEDnum].colorPause--; //reduce the LEDs pause number and check back next loop
    else {
      ledStatus[LEDnum].colorNum++;
      if (ledStatus[LEDnum].colorNum >= TOTALCOLORSWBIZ) ledStatus[LEDnum].colorNum = 0; //bring it back to color zero
      byte realColor = actualColorNum(ledStatus[LEDnum].colorNum);
      if (ledStatus[LEDnum].colorNum % (5) == 0) ledStatus[LEDnum].colorPause = random8(settings[0].frontDelay); //color is a key, assign random pause
      else ledStatus[LEDnum].colorPause = settings[0].frontFade; //color is a tween, assign a quick pause
      //
      if (briVal == 255) frontLEDs[LEDnum].setHSV( (allColors[0][realColor][0] + hueVal), allColors[0][realColor][1], allColors[0][realColor][2] );
      else frontLEDs[LEDnum].setHSV( (allColors[0][realColor][0] + hueVal), allColors[0][realColor][1], map8( briVal, 0, allColors[0][realColor][2] ) );
    }
  }
  if (frontRear == 1) {
    LEDnum = LEDnum + 80; //because ledStatus also holds status of the fronts (0-79), we need to start at 80
    if (ledStatus[LEDnum].colorPause != 0) ledStatus[LEDnum].colorPause--; //reduce the LEDs pause number and check back next loop
    else {
      ledStatus[LEDnum].colorNum++;
      if (ledStatus[LEDnum].colorNum >= TOTALCOLORSWBIZ) ledStatus[LEDnum].colorNum = 0; //bring it back to color zero
      byte realColor = actualColorNum(ledStatus[LEDnum].colorNum);
      if (ledStatus[LEDnum].colorNum % (5) == 0) ledStatus[LEDnum].colorPause = random8(settings[0].rearDelay); //color is a key, assign random pause
      else ledStatus[LEDnum].colorPause = settings[0].rearFade; //color is a tween, assign a quick pause
      //
      if (briVal == 255) rearLEDs[LEDnum - 80].setHSV( (allColors[1][realColor][0] + hueVal), allColors[1][realColor][1], allColors[1][realColor][2] );
      else rearLEDs[LEDnum - 80].setHSV( (allColors[1][realColor][0] + hueVal), allColors[1][realColor][1], map8( briVal, 0, allColors[1][realColor][2] ) );
    }
  }
}

//quick funciton to write all the current++++ settings to EEPROM in order
void writeSettingsToEEPROM() {
  EEPROM.write(0, settings[0].maxBri);
  EEPROM.write(1, settings[0].frontFade);
  EEPROM.write(2, settings[0].frontDelay);
  EEPROM.write(3, settings[0].frontHue);
  EEPROM.write(4, settings[0].rearFade);
  EEPROM.write(5, settings[0].rearDelay);
  EEPROM.write(6, settings[0].rearHue);
  EEPROM.write(7, settings[0].frontPalNum);
  EEPROM.write(8, settings[0].rearPalNum);
  EEPROM.write(9, settings[0].frontBri);
  EEPROM.write(10, settings[0].rearBri);
}
void readSettingsFromEEPROM() {
  settings[0] = {EEPROM.read(0), EEPROM.read(1), EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), EEPROM.read(6), EEPROM.read(7), EEPROM.read(8), EEPROM.read(9), EEPROM.read(10)};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  delay(50);
  
  #if (DEBUG>0)
  Serial.begin(115200);     
  Serial.println(F("Set up FastLED arrays")); 
  #endif 
      
  FastLED.addLeds<LED_TYPE, REAR_PIN, GRB>(rearLEDs, 96);
  fill_solid( rearLEDs, 96, CRGB(0, 0, 0));
  FastLED.addLeds<LED_TYPE, FRONT_PIN, GRB>(frontLEDs, 80);
  fill_solid( frontLEDs, 80, CRGB(0, 0, 0));

  pinMode(13, OUTPUT); digitalWrite(13, HIGH); //turn on AVR LED
  delay(100);

  //jumper is used to set front or rear adjustment mode (pull low to adjust)
  pinMode(FJUMP_PIN, INPUT); //avr board has a physical pulldown resistor
  pinMode(RJUMP_PIN, INPUT); //avr board has a physical pulldown resistor
  pinMode(PAL_PIN, INPUT_PULLUP); //uses internal pullup resistor

  //if eeprom settings look invalid (frontFade>100), or front jumper is pulled low, reset eeprom values to defaults
  if ((EEPROM.read(1) > 100) || (digitalRead(FJUMP_PIN) == 1)) {      
    #if (DEBUG>0)  
    Serial.println(F("eeprom 1="));  
    Serial.println(EEPROM.read(1));
    Serial.println(F("pin FJUMP_PIN="));
    Serial.println(digitalRead(FJUMP_PIN));
    Serial.println(F("eeprom invalid. writing...")); 
    #endif 
    writeSettingsToEEPROM();
    delay(999999999); //
  }
  #if (DEBUG>0)   
  else Serial.println(F("eeprom okay")); 
  #endif 

  #if (DEBUG>0)   
  Serial.println(F("reading eeprom")); 
  #endif 
  //read settings from eeprom...
  readSettingsFromEEPROM();

  #if (DEBUG>0)   
  Serial.println(F("calc front colors")); 
  #endif
  calculateAllColors(settings[0].frontPalNum, 0, settings[0].frontBri);
  for ( byte i = 0; i < 80; i++) {
    ledStatus[i] = {random8(TOTALCOLORSWBIZ), random8()};
    updateLED(pgm_read_byte(&frontLEDmap[i]), settings[0].frontHue, 0);
  }
  #if (DEBUG>0)   
  Serial.println(F("calc rear colors")); 
  #endif
  calculateAllColors(settings[0].rearPalNum, 1, settings[0].rearBri);
  for ( byte i = 0; i < 96; i++) {
    ledStatus[i + 80] = {random8(TOTALCOLORSWBIZ), random8()}; //assign the LED a random color number
    updateLED(pgm_read_byte(&rearLEDmap[i]), settings[0].rearHue, 1); //update that LED to actually set its color
  }
  /*#if (DEBUG>0)
  // print all the colors
  for(byte x=0;x<TOTALCOLORSWBIZ;x++) {
    Serial.println(String(x)+" : "+String(allColors[0][x][0]));
  }  
  Serial.println();
  for(byte x=0;x<TOTALCOLORSWBIZ;x++) {
    Serial.println(String(x)+" : "+String(allColors[1][x][0]));
  }  
  #endif */ 
  //delay(50);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  currentMillis = millis();

  //STATUS LED
  //check current status, indicate it using the onboard status LED
  if (adjMode == 1) statusDelay = statusDelayF;
  else if (adjMode == 3) statusDelay = statusDelayR;
  if (currentMillis - statusMillis >= statusDelay) {
    statusMillis = currentMillis;
    if (flipflop == 0) flipflop = 1;
    else flipflop = 0;
    if (adjMode != 0) {
      //for front adjustment mode, blink back & forth between blue and white
      if (flipflop == 0) {
        digitalWrite(13, HIGH);
      }
      else {
        digitalWrite(13, LOW);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// START ADJUSTMENTS
  //Where's me jumper? Set adjMode...
  // 0 for no adjustments, 1 for front, 2 for rear
  if (digitalRead(FJUMP_PIN) == 1) { //go to adjMode 1 (adj pots will tweak the front logic)
    if (adjMode != 1) {
      delay(250); //dumb delay
      if (digitalRead(FJUMP_PIN) == 1) { //double check after the delay (incase jumped by mistake)
        adjMode = 1; statusDelay = 500; //set the adjMode and delay time for the status LED
        adjMillis = millis(); //note when we start making adjustments
      }
    }
  }
  else if (digitalRead(RJUMP_PIN) == 1) { //go to adjMode 3 (adj pots will tweak the rear logic)
    if (adjMode != 3) {
      delay(250); //dumb delay
      if (digitalRead(RJUMP_PIN) == 1) { //double check after the delay (incase jumped by mistake)
        adjMode = 3; statusDelay = 500;
        adjMillis = millis();
      }
    }
  }
  if ((adjMode != 0) && (digitalRead(FJUMP_PIN) == 0) && (digitalRead(RJUMP_PIN) == 0)) { //go back to normal mode (adj pots disabled)
    delay(250); //delay for debounce
    if ((digitalRead(FJUMP_PIN) == 0) && (digitalRead(RJUMP_PIN) == 0)) { //double check after the delay (incase jumped by mistake)
      //adjMode just went back to 0
      delay(1000);
      adjMode = 0;
      statusDelay = 1500;
    }
  }
  if (adjMode > 0) {
    //are we adjusting front or rear? check it and then set to settings[0] values to match the trimpots current states
    if (prevAdjMode == 0) prevBrightness = map(analogRead(briPin), 0, 1023, MIN_BRIGHTNESS, settings[0].maxBri);
    if (adjMode == 1) {
      prevPalNum = settings[0].frontPalNum;
      settings[0].frontBri = map(analogRead(briPin), 0, 1023, MIN_BRIGHTNESS, settings[0].maxBri); //we want this to map to the settings[0].maxBri value (which is MAX_BRIGHTNESS by default)
      settings[0].frontDelay = analogRead(delayPin) / 4; //0-255
      settings[0].frontFade = analogRead(fadePin) / 128; //0-8
      settings[0].frontHue = analogRead(huePin) / 4; //0-255
      //if the PAL_PIN was low, switch the palette number
      if (!digitalRead(PAL_PIN)) {
        delay(100);
        settings[0].frontPalNum++;
        if (settings[0].frontPalNum >= PAL_COUNT) settings[0].frontPalNum = 0;
      }
      //if brightness or palette setting changed from this loop to last, recalculate allColors... (this is done sparingly so it doesn't delay the patterns)
      if (settings[0].frontBri != prevBrightness || settings[0].frontPalNum != prevPalNum) calculateAllColors(settings[0].frontPalNum, 0, settings[0].frontBri);
      prevBrightness = settings[0].frontBri;
    }
    else if (adjMode == 3) {
      prevPalNum = settings[0].rearPalNum;
      settings[0].rearBri = map(analogRead(briPin), 0, 1023, MIN_BRIGHTNESS, settings[0].maxBri);
      settings[0].rearDelay = analogRead(delayPin) / 4; //0-255
      settings[0].rearFade = analogRead(fadePin) / 128; //0-8
      settings[0].rearHue = analogRead(huePin) / 4; //0-255
      //if the PAL_PIN was low, switch the palette number
      if (!digitalRead(PAL_PIN)) {
        delay(500);
        settings[0].rearPalNum++;
        if (settings[0].rearPalNum >= PAL_COUNT) settings[0].rearPalNum = 0;
      }
      //if brightness setting changed from this loop to last, recalculate allColors
      if (settings[0].rearBri != prevBrightness || settings[0].rearPalNum != prevPalNum) calculateAllColors(settings[0].rearPalNum, 1, settings[0].rearBri);
      prevBrightness = settings[0].rearBri;
    }
  }
  else if (adjMode == 0 && prevAdjMode != 0) {
    //adjMode just went back to 0, write settings to eeprom
    writeSettingsToEEPROM();
  }
  prevAdjMode = adjMode;
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// END ADJUSTMENTS

  for ( byte i = 0; i < 80; i++) updateLED(pgm_read_byte(&frontLEDmap[i]), settings[0].frontHue, 0);
  for ( byte i = 0; i < 96; i++) updateLED(pgm_read_byte(&rearLEDmap[i]), settings[0].rearHue, 1);
  FastLED.show();

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  finn
