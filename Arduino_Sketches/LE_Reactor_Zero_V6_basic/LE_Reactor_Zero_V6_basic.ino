// Paul Murphy 2020-09-22
// Basic sketch with scrolling text functions (via Jawalite serial commands) using standard Adafruit libraries
// Updated for V6 Reactor Zero board (with single "Adjust" button, no selector switch) and options for 96 and 112 LED boards.
// For more advanced functionalilty, see the firmware developed by Neil Hutchison here : https://github.com/nhutchison/LogicEngine
/*
 * 
 * Required Arduino Libraries:
 *  - Adafruit_NeoPixel
 *  - Adafruit_GFX
 *  - Adafruit_NeoMatrix
 *  - LEDControl
 *  Also Required For Reactor Zero board:
 *  - FlashStorage
 * 
 * Recent Changes:
 *  - Added support for the new 112 LED rear logic PCB (this is enabled by the RLD112 in settings.h)
 *  - Added functionality for the V6 Reactor Zero board, which has a single "Adjust" button, no selector switch.
 *  - Removed options for different LED libraries; we're focused on the Adafruit stuff now.
 *  - Moved all settings that users may want to tweak to separate settings.h file.
 *  - Added two fonts (5px for FLD, 4px staggered for RLD).
 *  - Moved helper functions to functions.h
 * 
 */
 
#include "settings.h"

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
  #warning [not an actual error dont worry] - Compiling for a Teensy 3.1 or 3.2
#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)  || defined(__AVR_ATmega168__)  
  #warning [not an actual error dont worry] - Compiling for an AT328P (Arduino Uno etc)
#elif defined(__SAMD21G18A__)
  #warning [not an actual error dont worry] - Compiling for Reactor Zero
#elif defined(ARDUINO_ARCH_ESP32)
  #warning [not an actual error dont worry] - Compiling for ESP32  
#else
  #error UNRECOGNIZED BOARD! Are you sure you chose the correct Board from the Tools menu?
#endif



/////////////////////////////////////
//some general stuff to start with...
#define FrontLEDCount 80
#if RLD112 == true
#define RearLEDCount 112
#else
#define RearLEDCount 96
#endif
unsigned long currentMillis,prevFront1Millis,prevFront2Millis,prevRearMillis,prevEffectStep,effectEndTime;
#define TIMECHECKMS 10000
bool frontRear;
byte displayMode[4]={0,0,0,0}; //0=blinkies, 1=scrollingtext
byte effectRunning;
bool fadeUpDown;



/////////////////
//text related...
#define CMD_MAX_LENGTH 64 // maximum number of characters in a command (63 chars since we need the null termination)
#define MAXSTRINGSIZE 64  // maximum number of letters in a logic display message
char cmdString[CMD_MAX_LENGTH]; 
char ch;
bool command_available; 
bool doingSerialStuff;
unsigned long serialMillis = millis();
#define serialMillisWait 10 //this is how many milliseconds we hold off from sending data to LEDs if serial data has started coming in
int scrollPositions[]={0,8,8,(RearLEDCount/4)}; // starting values for text scrolling positions on each display.
long textScrollCount[3]; // 0,1, and 2 are used in scrollText for each display, counts how many times the string has completely scrolled
char logicText[3][MAXSTRINGSIZE+1]={"R2","D2","R2-D2"}; // memory for the text strings to be displayed, add one for the NULL character at the end
/*int xPosFront1=8;  //front text position while scrolling
int xPosFront2=8; //(initially width of front logic)
int xPosRear=24; //(initially width of rear logic)*/


///////////////////////
//status LED related...
bool statusFlipFlop=0;
bool prevStatusFlipFlop=1;
#define slowBlink 800 //number of millis between Status LED changes in normal mode
#define fastBlink 100  //number of millis between Status LED changes in Adjust mode
unsigned long prevFlipFlopMillis;
int statusFlipFlopTime = slowBlink;


//////////////////////////////////
//adjustment mode related stuff...
unsigned int palPinLoops; //used to count how long the Pallet button is held
bool palPinStatus = 1;
bool prevPalPinStatus = 1;
byte adjMode, prevAdjMode, startAdjMode;
unsigned int adjLoops;
#define adjLoopMax 90000 //if we're left in Adjust mode for this many loops, we go back to normal mode
#define adjLoopMin 500   //if we just came from Adjust mode, but were only there momentarily, we don't save changes
int startTrimpots[4]; //will hold trimpot values when adjustments start being made
bool trimEnabled[4]; //during adjustment, if trimpot has moved beyond specified threshold it will be enabled here
int loopTrimpots[4]; //will hold trimpot values when adjustments start being made
bool adjEnabled[4]; //tells us if a trimpot has been adjusted beyond adj_threshold
byte adjThreshold = 5;
byte onOff[4]={0,1,1,1}; //turns on/off each logic display


//////////////////////////////////
//without RTCZero, millis() counts will be all off on the Reactor Zero
//this shouldn't be necesaary if using a Teensy, AVR or ESP32
/*#if defined(__SAMD21G18A__)
  #include <RTCZero.h>
  RTCZero rtc;
#endif*/


////////////////////////////////////////
//a structure to keep our settings in...
typedef struct {
  unsigned int writes; //keeps a count of how many times we've written settings to flash
  byte maxBri;
  int frontDelay; byte frontFade; byte frontBri; byte frontHue; byte frontPalNum; byte frontDesat;
  int rearDelay;  byte rearFade;  byte rearBri;  byte rearHue;  byte rearPalNum; byte rearDesat;
  byte frontScrollSpeed; byte rearScrollSpeed;
} Settings;
#if defined(__SAMD21G18A__)
  // the Reactor Zero doesn't have EEPROM, so we use the FlashStorage library to store settings persistently in flash memory
  #include <FlashStorage.h> //see StoreNameAndSurname example
  FlashStorage(my_flash_store, Settings); // Reserve a portion of flash memory to store Settings
  Settings mySettings;   //create a mySettings variable structure in SRAM
  Settings tempSettings; //create a temporary variable structure in SRAM
#endif  


//////////////////////////////////////////////////////////////////////////////////////
//a struct that holds the current color number and pause value of each LED (when pause
//value hits 0, the color number gets changed). To save SRAM, we don't store the
//"direction" anymore, instead we pretend that instead of having 16 colors, we've got
//31 (16 that cross-fade up, and 15 "bizarro" colors that cross-fade back in reverse)
struct LEDstat {
  byte colorNum;
  int colorPause;
};
LEDstat frontLEDstatus[FrontLEDCount];
LEDstat rearLEDstatus[RearLEDCount];
#define TOTALCOLORS (4+(TWEENS*(3)))
#define TOTALCOLORSWBIZ ((TOTALCOLORS*2)-2)
byte allColors[2][TOTALCOLORS][3]; //the allColor array will comprise of two color palettes


////////////////////////////////////////
//NeoPixel related stuff...
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
Adafruit_NeoMatrix frontMatrix = Adafruit_NeoMatrix(8, 5, 1, 2, FRONT_PIN,
  NEO_TILE_TOP   + NEO_TILE_LEFT   + NEO_TILE_ROWS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);
#if RLD112 == true  
  #include <Adafruit_DotStarMatrix.h>
  #include <Adafruit_DotStar.h>
  Adafruit_DotStarMatrix rearMatrix = Adafruit_DotStarMatrix((RearLEDCount/4), 4, REAR_DAT_PIN, REAR_CLK_PIN,
    NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
    DOTSTAR_BGR);
#else
  Adafruit_NeoMatrix rearMatrix = Adafruit_NeoMatrix((RearLEDCount/4), 4, REAR_PIN,
    NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
    NEO_GRB + NEO_KHZ800);
#endif  
    
Adafruit_NeoPixel statusLED = Adafruit_NeoPixel(1, STATUSLED_PIN, NEO_GRB + NEO_KHZ800);
bool updateLEDs[2];


////////////////////////
//Teeces PSI's...
#if (TEECESPSI>0)
  const int PSIpause[2] = { 3000, 6000 };
  const byte PSIdelay[2]PROGMEM = { 25, 35 };
  #include <LedControl.h>
  #undef round
  LedControl lcChain = LedControl(TEECES_D_PIN, TEECES_C_PIN, TEECES_L_PIN, 2);
  byte PSIstates[2] = { 0, 0 };
  unsigned long PSItimes[2] = { millis(), millis() };
  unsigned int PSIpauses[2] = { 0, 0 };
#endif


////////////////////////
//some final includes...
#include "font_fld_5px.h"
#include "font_rld_staggered_left.h"
#include "functions.h"


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
void setup() {

  #if (DEBUG>0)
    delay(2000);   
    DEBUG_SERIAL.begin(BAUDRATE);
  #endif 
  JEDI_SERIAL.begin(BAUDRATE);

  /*#if defined(__SAMD21G18A__)
    rtc.begin();
    rtc.setTime(0, 0, 0);
  #endif*/

  mySettings = my_flash_store.read();
  if (mySettings.writes == false || mySettings.rearScrollSpeed == false) {
    #if (DEBUG>0)
        DEBUG_SERIAL.println(F("no settings"));
    #endif
    mySettings = { 1, MAX_BRIGHTNESS,
                   DFLT_FRONT_DELAY, DFLT_FRONT_FADE, DFLT_FRONT_BRI, DFLT_FRONT_HUE, DFLT_FRONT_PAL, DFLT_FRONT_DESAT,
                   DFLT_REAR_DELAY,  DFLT_REAR_FADE,  DFLT_REAR_BRI,  DFLT_REAR_HUE,  DFLT_REAR_PAL, DFLT_REAR_DESAT,
                   DFLT_FRONT_SCROLL,DFLT_REAR_SCROLL
                 };
    my_flash_store.write(mySettings);
    mySettings = my_flash_store.read();
    #if (DEBUG>0)
          DEBUG_SERIAL.println(F("dflts wrote"));
    #endif
  }
  #if (DEBUG>0)
    DEBUG_SERIAL.println(mySettings.writes); //keeps a count of how many times we've written settings to flash
    DEBUG_SERIAL.println(mySettings.maxBri);
    DEBUG_SERIAL.println(mySettings.frontDelay);
    DEBUG_SERIAL.println(mySettings.frontFade);
    DEBUG_SERIAL.println(mySettings.frontBri);
    DEBUG_SERIAL.println(mySettings.frontHue);
    DEBUG_SERIAL.println(mySettings.frontPalNum);
    DEBUG_SERIAL.println(mySettings.frontDesat);
    DEBUG_SERIAL.println(mySettings.rearDelay); 
    DEBUG_SERIAL.println(mySettings.rearFade);
    DEBUG_SERIAL.println(mySettings.rearBri);
    DEBUG_SERIAL.println(mySettings.rearHue);
    DEBUG_SERIAL.println(mySettings.rearPalNum);
    DEBUG_SERIAL.println(mySettings.rearDesat);
    DEBUG_SERIAL.println(mySettings.frontScrollSpeed);
    DEBUG_SERIAL.println(mySettings.rearScrollSpeed);
  #endif
  
  frontMatrix.begin();
  frontMatrix.setTextWrap(false);
  frontMatrix.setBrightness(40);
  frontMatrix.setTextColor(frontMatrix.Color(0,0,100));
  frontMatrix.setFont(&fld_5px);
  
  rearMatrix.begin();
  rearMatrix.setTextWrap(false);
  rearMatrix.setBrightness(40);
  rearMatrix.setTextColor(rearMatrix.Color(0,100,0));
  rearMatrix.setFont(&rld_4px);

  statusLED.begin();
  statusLED.setPixelColor(0, 0, 2, 0); //green
  statusLED.show();

  //generate all the logic colors!
  calcColors(mySettings.frontPalNum, 0);
  calcColors(mySettings.rearPalNum, 1);

  //assign each logic LED a random color and pause value
  for ( byte LEDnum = 0; LEDnum < FrontLEDCount; LEDnum++) {
    frontLEDstatus[LEDnum].colorNum = random(TOTALCOLORSWBIZ);
    frontLEDstatus[LEDnum].colorPause = random(mySettings.frontDelay);
  }
  for ( byte LEDnum = 0; LEDnum < RearLEDCount; LEDnum++) {
    rearLEDstatus[LEDnum].colorNum = random(TOTALCOLORSWBIZ);
    rearLEDstatus[LEDnum].colorPause = random(mySettings.rearDelay);
  }

  #if (TEECESPSI>0)
      lcChain.shutdown(0, false); //take the device out of shutdown (power save) mode
      lcChain.clearDisplay(0);
      lcChain.shutdown(1, false); //take the device out of shutdown (power save) mode
      lcChain.clearDisplay(1);
      lcChain.setIntensity(0, FPSIbright); //Front PSI
      lcChain.setIntensity(1, RPSIbright); //Rear PSI
      #if (TEECESPSI>1)
        while (1==1) {
          setPSIstate(0, 0);
          setPSIstate(1, 0);
          delay(1000);
          setPSIstate(0, 6);
          setPSIstate(1, 6);
          delay(1000);
        }        
      #endif
    #endif

  pinMode(FADJ_PIN, INPUT_PULLUP); //use internal pullup resistors of Teensy
  pinMode(RADJ_PIN, INPUT_PULLUP);
  if (digitalRead(RADJ_PIN) == 0 or digitalRead(FADJ_PIN) == 0) startAdjMode = 1; //adj switch isn't centered!
  pinMode(PAL_PIN, INPUT_PULLUP);  
  
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
void loop() {
  currentMillis = millis();

  checkJediSerial();

  if (effectRunning!=0 && effectEndTime<currentMillis) {
    //we've reached the end of an effect, put settings back to their stored values
    loadSettings();
    effectRunning=0;
    onOff[1]=1;
    onOff[2]=1;
    onOff[3]=1;
  }

  if (effectRunning==2 && currentMillis-prevEffectStep>100) {
    //ALARM
    //alternate front logics on/off
    if (onOff[1]==0) {
      onOff[1]=1;
      onOff[2]=0;
      frontMatrix.fillRect(0,5, 8,10, 0); //black out bottom front logic
    }
    else {
      onOff[1]=0;
      onOff[2]=1;
      frontMatrix.fillRect(0,0, 8,5, 0); //black out top front logic
    }
    prevEffectStep=currentMillis;
  }

  if (effectRunning==6) {
    //LEIA
    //fade up and down brightness
    if (fadeUpDown==0) {
      mySettings.frontBri=mySettings.frontBri-5;;
      mySettings.rearBri=mySettings.rearBri-5;;
    }
    else {
      mySettings.frontBri=mySettings.frontBri+5;
      mySettings.rearBri=mySettings.frontBri+5;
    }
    //if (mySettings.frontBri>=tempSettings.frontBri) {
    if (mySettings.frontBri>=255) {  
      fadeUpDown=0;
    }
    if (mySettings.frontBri==0) {
      fadeUpDown=1;
      mySettings.frontHue=mySettings.frontHue+50;
      mySettings.rearHue=mySettings.rearHue+50;
    }
    calcColors(mySettings.frontPalNum, 0);
    calcColors(mySettings.rearPalNum, 1);
  }


  //checkAdjSwitch(); //checks the switch and sets adjMode and flipFlopMillis
  setStatusLED(); //blinks the status LED back and forth
  if (adjMode != 0) {
    adjLoops++;
    compareTrimpots(adjMode);
  }
  int palBut = checkPalButton();
  #if (DEBUG>=3)
      if (palBut>1) {
        DEBUG_SERIAL.print(F("palBut="));
        DEBUG_SERIAL.println(palBut);
      }
  #endif
  if (palBut >= 300) {
    #if (DEBUG>0)
      DEBUG_SERIAL.println(F("2 sec rule"));
    #endif
    factorySettings();
    calcColors(mySettings.frontPalNum, 0);
    calcColors(mySettings.rearPalNum, 1);
    my_flash_store.write(mySettings);
    statusBlink(6, 100, 1, 0, 2); //blink purple 6 times fast
    adjMode = 0;
    statusFlipFlopTime = slowBlink;
  }
  else if (adjMode == 0 && palBut >= 50) {
    adjLoops=0;
    adjMode=3;
    statusFlipFlopTime = fastBlink;
    #if (DEBUG>=3)
      DEBUG_SERIAL.println(F("adjMode 3"));
    #endif
  }
  else if (adjMode == 0 && palBut >= 5) {
    adjLoops=0;
    adjMode=1;
    statusFlipFlopTime = fastBlink;
    #if (DEBUG>=3)
      DEBUG_SERIAL.println(F("adjMode 1"));
    #endif
  }
  else if (adjMode != 0 && palBut >= 50) {
    //pal button was held, save our settings *************************************************************
    adjMode = 0;
    statusFlipFlopTime = slowBlink;
    #if (DEBUG>=3)
      DEBUG_SERIAL.println(F("Saving settings"));
    #endif
    statusBlink(2, 250, 1, 0, 2); //blink purple 2 times
    mySettings.writes++;
    saveSettings();   
  }
  else if (adjMode != 0 && palBut >= 3) {
    //change up the color palette used for this logic display
    changePalNum(adjMode);
  }
  


  if (doingSerialStuff==1 && (millis()-serialMillis>serialMillisWait) ) doingSerialStuff=0; //enough time has passed since we saw some serial data, send data to the LEDs again
  if (doingSerialStuff==0) {
  
      if (displayMode[1]==0) {
        //UPDATE TOP FRONT LOGIC...
        if (onOff[1]==1) {
          for ( byte LEDnum = 0; LEDnum < 40; LEDnum++) {
            if (frontLEDstatus[LEDnum].colorPause != 0)  frontLEDstatus[LEDnum].colorPause--;
            else {
              frontLEDstatus[LEDnum].colorNum++;
              if (frontLEDstatus[LEDnum].colorNum >= TOTALCOLORSWBIZ) frontLEDstatus[LEDnum].colorNum = 0; //bring it back to color zero
              if (frontLEDstatus[LEDnum].colorNum % (TWEENS + 1) == 0) frontLEDstatus[LEDnum].colorPause = random(mySettings.frontDelay); //color is a key, assign random pause
              else frontLEDstatus[LEDnum].colorPause = mySettings.frontFade; //color is a tween, assign a quick pause
              //now set the actual color of this LED, you big dummy
              frontMatrix.setPixelColor(LEDnum, allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][0] , allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][1] , allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][2]); 
              updateLEDs[0]=1;
            }
          }
        }    
      }

      //onOff[2]==0; //temporarily disabling front updates to see if speed increases 
      if (displayMode[2]==0) {
        if (onOff[2]==1) {
          for ( byte LEDnum = 40; LEDnum < 80; LEDnum++) {
            if (frontLEDstatus[LEDnum].colorPause != 0)  frontLEDstatus[LEDnum].colorPause--;
            else {
              frontLEDstatus[LEDnum].colorNum++;
              if (frontLEDstatus[LEDnum].colorNum >= TOTALCOLORSWBIZ) frontLEDstatus[LEDnum].colorNum = 0; //bring it back to color zero
              if (frontLEDstatus[LEDnum].colorNum % (TWEENS + 1) == 0) frontLEDstatus[LEDnum].colorPause = random(mySettings.frontDelay); //color is a key, assign random pause
              else frontLEDstatus[LEDnum].colorPause = mySettings.frontFade; //color is a tween, assign a quick pause
              //now set the actual color of this LED, you big dummy
              frontMatrix.setPixelColor(LEDnum, allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][0] , allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][1] , allColors[0][actualColorNum(frontLEDstatus[LEDnum].colorNum)][2]);
              updateLEDs[0]=1;
            }   
          }
        }  
      }
      //if (displayMode[1]==0||displayMode[2]==0) frontMatrix.show();
      //if (displayMode[1]==0||displayMode[2]==0) updateLEDs[0]=1;
      
      if (displayMode[3]==0) {  
        //UPDATE REAR LOGIC...
        if (onOff[3]==1) {
          for ( byte LEDnum = 0; LEDnum < RearLEDCount; LEDnum++) {
            if (rearLEDstatus[LEDnum].colorPause != 0)  rearLEDstatus[LEDnum].colorPause--;
            else {
              rearLEDstatus[LEDnum].colorNum++;
              if (rearLEDstatus[LEDnum].colorNum >= TOTALCOLORSWBIZ) rearLEDstatus[LEDnum].colorNum = 0; //bring it back to color zero
              if (rearLEDstatus[LEDnum].colorNum % (TWEENS + 1) == 0) rearLEDstatus[LEDnum].colorPause = random(mySettings.rearDelay); //color is a key, assign random pause
              else rearLEDstatus[LEDnum].colorPause = mySettings.rearFade; //color is a tween, assign a quick pause
              //now set the actual color of this LED, you big dummy
              rearMatrix.setPixelColor(LEDnum, allColors[1][actualColorNum(rearLEDstatus[LEDnum].colorNum)][0] , allColors[1][actualColorNum(rearLEDstatus[LEDnum].colorNum)][1] , allColors[1][actualColorNum(rearLEDstatus[LEDnum].colorNum)][2]); 
              updateLEDs[1]=1;
            }
          }
        } 
        //rearMatrix.show();
      }
    
      if (displayMode[1]==1) {
        //scroll text on top front logic
        if (currentMillis-prevFront1Millis>mySettings.frontScrollSpeed) {
          int msgLength=(strlen(logicText[0]))*5;
          frontMatrix.fillRect(0,0, 8,5, 0); //black out top front logic
          frontMatrix.setCursor(scrollPositions[1], 4);
          frontMatrix.print(logicText[0]);          
          if( --scrollPositions[1] < -msgLength ) {
            //all done scrolling, set things back to normal
            scrollPositions[1] = 8;
            displayMode[1]=0;
            frontMatrix.setTextColor(frontMatrix.Color(0,0,100));
          }
          updateLEDs[0]=1;
          //frontMatrix.show();
          prevFront1Millis=currentMillis;
        }
      }
    
      if (displayMode[2]==1) {
        //scroll text on bottom front logic
        if (currentMillis-prevFront2Millis>mySettings.frontScrollSpeed) {
          int msgLength=(strlen(logicText[1]))*5;
          frontMatrix.fillRect(0,5, 8,10, 0); //black out bottom front logic
          frontMatrix.setCursor(scrollPositions[2], 9);
          frontMatrix.print(logicText[1]);          
          if( --scrollPositions[2] < -msgLength ) {
            //all done scrolling, set things back to normal
            scrollPositions[2] = 8;
            displayMode[2]=0;
            frontMatrix.setTextColor(frontMatrix.Color(0,0,100));
          }
          updateLEDs[0]=1;
          //frontMatrix.show();
          prevFront2Millis=currentMillis;
        }
      }
    
      if (displayMode[3]==1) {
        //scroll text on rear logic
        if (currentMillis-prevRearMillis>mySettings.rearScrollSpeed) {
          int msgLength=(strlen(logicText[2]))*4;
          rearMatrix.fillRect(0,0, (RearLEDCount/4),4, 0); //black out rear logic
          rearMatrix.setCursor(scrollPositions[3], 3);
          rearMatrix.print(logicText[2]);          
          if( --scrollPositions[3] < -msgLength ) {
            //all done scrolling, set things back to normal
            scrollPositions[3] = (RearLEDCount/4);
            displayMode[3]=0;
            rearMatrix.setTextColor(rearMatrix.Color(0,100,0));
          }
          updateLEDs[1]=1;
          //rearMatrix.show();
          prevRearMillis=currentMillis;
        }
      }
    
      if (updateLEDs[0]==1) {
        updateLEDs[0]=0;
        frontMatrix.show();
      }
      if (updateLEDs[1]==1) {
        updateLEDs[1]=0;
        rearMatrix.show();
      }
  
  }
  
  
  if (currentMillis-prevFlipFlopMillis>=statusFlipFlopTime) {
    if (statusFlipFlop==0) {
      prevStatusFlipFlop=0;
      statusFlipFlop=1;
    }
    else {
      prevStatusFlipFlop=1;
      statusFlipFlop=0;
    }
    prevFlipFlopMillis=currentMillis;
  }
  setStatusLED();
  statusLED.show();


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// TEECES PSI STUFF
    #if (TEECESPSI==1)
      for (byte PSInum = 0; PSInum < 2; PSInum++) {
        if (millis() - PSItimes[PSInum] >= PSIpauses[PSInum]) {
          //time's up, do something...
          PSIstates[PSInum]++;
          if (PSIstates[PSInum] == 12) PSIstates[PSInum] = 0;
          if (PSIstates[PSInum] != 0 && PSIstates[PSInum] != 6) {
            //we're swiping...
            PSIpauses[PSInum] = pgm_read_byte(&PSIdelay[PSInum]);
          }
          else {
            //we're pausing
            PSIpauses[PSInum] = random(PSIpause[PSInum]);
            //decide if we're going to get 'stuck'
            if (random(100) <= PSIstuck) {
              if (PSIstates[PSInum] == 0) PSIstates[PSInum] = random(1, 3);
              else PSIstates[PSInum] = random(3, 5);
            }
          }
          setPSIstate(PSInum, PSIstates[PSInum]);
          PSItimes[PSInum] = millis();
        }
      }
    #endif
  /*if (currentMillis-prevFrontMillis>mySettings.frontScrollSpeed) {
    frontMatrix.fillScreen(0);
    frontMatrix.setCursor(xf, 4);
    frontMatrix.print(F("HOWDY"));    
    if(--xf < -36) {
      xf = frontMatrix.width();
    }
    frontMatrix.show();
    prevFrontMillis=currentMillis;
  }
  if (currentMillis-prevRearMillis>mySettings.rearScrollSpeed) {
    rearMatrix.fillScreen(0);    
    rearMatrix.setCursor(xr, 3);
    rearMatrix.print(F("01234 STAR WARS"));  
    if(--xr < -96) {
      xr = rearMatrix.width();
    }   
    rearMatrix.show();
    prevRearMillis=currentMillis;
  } */
    
    
    
  
}
