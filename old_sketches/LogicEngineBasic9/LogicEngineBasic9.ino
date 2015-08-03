//
//  RSeries Logic Engine AVR : Basic Sketch by Paul Murphy
//  ======================================================
//  This sketch provides basic functionality for the RSeries Logic Engine hardware.
//  Does not include microSD reading or I2C communication.
//
//  Uses the FastSPI_LED2 library : https://code.google.com/p/fastspi/downloads/detail?name=FastSPI_LED2.RC5.zip
//
//   revision history...
//   2014-03-09 : Added code for Teeces PSI V2
//   2014-01-15 : Optimizing memory to make way for the SD card's wretched 512 byte buffer.
//                Moved rldMap, fldMap, rldColors & fldColors out of SRAM and into flash memory.
//                Eliminated oColors array (adjusted hues are now calculated individually by updateLED instead of all at once).
//                Added debug option to report available SRAM.
//                Need to write the makeColors function and a few small helpers for updateLED.    
//                Need to reduce LEDstat size (direction can be combined with color number to save 64 bytes)
//   2014-01-12 : Fixed prototype FLD bug
//   2014-01-08 : Removed debug options to free up some RAM. Fixed rldMap. Fixed fldMap.
//   2014-01-07 : Added 'Testpattern Mode' to aid assembly of the Rear Logic
//

/*

  Dr.Paul explains Colors, LED Direction and (if you're lucky) Midichlorians:
  
  We give the sketch a selection of 'Key' colors. For the Rear Logic we use 5 colors; black, dark green, green, yellow, red.
  The sketch takes those Key colors and generates several 'Tween' colors that fade between each Key color. These colors are
  all put in order in a big array called AllColors. The color elements themselves are HSV values (Hue, Saturation, Value).
  
  During the sketches main loop(), each pixel is cycled through all these colors, so thanks to the tween colors they appear to
  fade from color to color.  As each Key color is reached the pixel is paused for a slightly random number of loops; this
  randomness causes the color patterns to constantly evolve, so over time you will see different patterns emerge.
  
  Say we have 3 key colors and 1 tween color. This creates an AllColors array of 5 unique colors...
    0: Black
    1: Tween
    2: Red
    3: Tween
    4: Yellow
  After the 5th color (Yellow) is reached, we want to switch direction...
    5: Tween (3)
    6: Red   (2)
    7: Tween (1) 
  So even though we only use 5 unique colors, each pixel loops through 8 color changes.
  Sometimes we might refer to Color 6; there is no Color 6 in our AllColors array, but we can figure it out...
  if (colorNum>=numColors) actualColorNum=numColors-(colorNum-numColors)-2;
  using the above example numbers...
  if (6>=5) colorNum=5-(6-5)-2; // so color 6 is really 2 (Red) when looping through the array in reverse
  
  To put it simply, if colorNum >= numColors then the pixel is moving in reverse through the color array.
*/

#define PSIbright 12
int psiRed=2500;    //how long front PSI stays red  (or yellow)
int psiBlue=1700;   //how long front PSI stays blue (or green)
#define rbSlide 125 // mts - time to transition between red and blue

//  the TOGGLEPIN is used if there's no SD card. if this pin is jumped to +5V,
//  then we'll assume this is the RLD, otherwise we'll assume this is the FLD
#define TOGGLEPIN 4
//  the SPEEDPIN enables speed adjustment via the pots via the pause & delay
#define SPEEDPIN 3

//debug will print a bunch of stuff to serial. useful, but takes up valuable memory and may slow down routines a little
#define DEBUG 0 //0=off, 1=only report available memory, 2=report lots of stuff, 3=slow the main loop down

#define PCBVERSION 2 // 1 if your LED boards are from the first run (48 LEDs per PCB), otherwise set this to 2

#define LOOPCHECK 20 //number of loops after which we'll check the pot values
//                                                                                           Variable Sizes in SRAM:
#include "FastSPI_LED2.h"
#define DATA_PIN 6
CRGB leds[96];      // structure used by the FastSPI_LED library                                    288 bytes

byte Tweens=6;     // number of tween colors to generate                                              1 byte
byte tweenPause=7; // time to delay each tween color (aka fadeyness)                                  1 byte
int keyPause=350; // time to delay each key color (aka pauseyness)                                    2 bytes
byte maxBrightness=255; // 0-255, no need to ever go over 128                                         1 byte

int keyPin = A0; //analog pin to read keyPause value                                                  2 bytes
int tweenPin = A1; //analog pin to read tweenPause value                                              2 bytes
int briPin = A2; //analog pin to read Brightness value                                                2 bytes
int huePin = A3; //analog pin to read Color/Hue shift value                                           2 bytes

byte totalColors,Keys; //                                                                             2 bytes

byte AllColors[45][3]; // a big array to hold all original KeyColors and all Tween colors           135 bytes
byte LEDstat[96][3]; // an array holding the current color number of each LED                       288 bytes
unsigned int LEDpause[96]; // holds pausetime for each LED (how many loops before changing colors)  192 bytes
boolean speeds=0; //0 for preset, 1 for tweakable (depends on speedpin)                               1 byte
boolean logic=0; //0 for fld, 1 for rld (depends on togglepin)                                        1 byte

unsigned int loopCount; // used during main loop                                                      2 bytes
byte briVal,prevBri,briDiff,hueVal; // global variables used during main loop and functions           4 bytes
#if (DEBUG>1)
unsigned long time; //used to calculate loops-per-second in debug mode
#endif

//our LEDs aren't in numeric order (FLD is wired serpentine and RLD is all over the place!)
//so we can address specific LEDs more easily, we'll map them out here...
#include <avr/pgmspace.h> //to save SRAM, this stuff all gets stored in flash memory

#if PCBVERSION<2

  //mapping for first RLD (two PCBs soldered together)
  const byte rldMap[]PROGMEM = {
   0, 1, 2, 3, 4, 5, 6, 7,48,49,50,51,52,53,54,55,
  15,14,13,12,11,10, 9, 8,63,62,61,60,59,58,57,56,
  16,17,18,19,20,21,22,23,64,65,66,67,68,69,70,71,
  31,30,29,28,27,26,25,24,79,78,77,76,75,74,73,72,
  32,33,34,35,36,37,38,39,80,81,82,83,84,85,86,87,
  47,46,45,44,43,42,41,40,95,94,93,92,91,90,89,88};
  //mapping for FLD boards from first run (with 48 LEDs per PCB)
  const byte fldMap[]PROGMEM = {
  15,14,13,12,11,10, 9, 8,
  16,17,18,19,20,21,22,23,
  31,30,29,28,27,26,25,24,
  32,33,34,35,36,37,38,39,
  47,46,45,44,43,42,41,40, //
  88,89,90,91,92,93,94,95, //
  87,86,85,84,83,82,81,80,
  72,73,74,75,76,77,78,79,
  71,70,69,68,67,66,65,64,
  56,57,58,59,60,61,62,63};
  
#else

  //mapping for single RLD PCB (second parts run on)...
  const byte rldMap[]PROGMEM = {
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
  31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,
  64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80};
  //mapping for newer FLD PCBs (40 LEDs per PCB)...
  const byte fldMap[]PROGMEM = { 
   0, 1, 2, 3, 4, 5, 6, 7,
  15,14,13,12,11,10, 9, 8,
  16,17,18,19,20,21,22,23,
  31,30,29,28,27,26,25,24,
  32,33,34,35,36,37,38,39,
  40,41,42,43,44,45,46,47,
  55,54,53,52,51,50,49,48,
  56,57,58,59,60,61,62,63,
  71,70,69,68,67,66,65,64,
  72,73,74,75,76,77,78,79};
  
#endif

//default colors also get stored in flash memory...
const byte fldColors[6][3]PROGMEM = { {170,0,0} , {170,255,54} , {170,255,120} , {166,255,200} , {154,84,150} , {174,0,200} };
const byte rldColors[5][3]PROGMEM = { {87,0,0} , {87,206,105} , {79,255,184} , {18,255,250} , {0,255,214} };

//experimental microSD stuff
/*#include <SD.h>
const int chipSelect = 10; //digital pin used for microSD
File myFile;*/

// TEECES PSI CODE...
#include <LedControl.h>
LedControl lcPSI=LedControl(7,8,9,1); //Data,Clock,Load,DevNum  (pins go GND,+5V,L,C,D)
#define HPROW 5
  class PSI {
  int stage; //0 thru 6
  int inc;
  int stageDelay[7];
  int cols[7][5];
  int randNumber; //a random number to decide the fate of the last stage

  unsigned long timeLast;
  int device;

  public:
  
  PSI(int _delay1, int _delay2, int _delay3, int _device)
  {
    device=_device;
    
    stage=0;
    timeLast=0;
    inc=1;
    
    cols[0][0] = B10101000;
    cols[0][1] = B01010100;
    cols[0][2] = B10101000;
    cols[0][3] = B01010100;
    cols[0][4] = B10101000;
    
    cols[1][0] = B00101000; //R B R B R B
    cols[1][1] = B11010100; //B R B R B R
    cols[1][2] = B00101000; //R B R B R B
    cols[1][3] = B11010100; //B R B R B R
    cols[1][4] = B00101000; //R B R B R B

    cols[2][0] = B01101000;
    cols[2][1] = B10010100;
    cols[2][2] = B01101000;
    cols[2][3] = B10010100;
    cols[2][4] = B01101000;
    
    cols[3][0] = B01001000;
    cols[3][1] = B10110100;
    cols[3][2] = B01001000;
    cols[3][3] = B10110100;
    cols[3][4] = B01001000;
    
    cols[4][0] = B01011000;
    cols[4][1] = B10100100;
    cols[4][2] = B01011000;
    cols[4][3] = B10100100;
    cols[4][4] = B01011000;
    
    cols[5][0] = B01010000;
    cols[5][1] = B10101100;
    cols[5][2] = B01010000;
    cols[5][3] = B10101100;
    cols[5][4] = B01010000;
    
    cols[6][0] = B01010100;
    cols[6][1] = B10101000;
    cols[6][2] = B01010100;
    cols[6][3] = B10101000;
    cols[6][4] = B01010100;
    
    stageDelay[0] = _delay1 - _delay3;
    stageDelay[1] = _delay3/5;
    stageDelay[2] = _delay3/5;
    stageDelay[3] = _delay3/5;
    stageDelay[4] = _delay3/5;
    stageDelay[5] = _delay3/5;
    stageDelay[6] = _delay2 - _delay3;
  }
  
  void Animate(unsigned long elapsed, LedControl control)
  {
    if ((elapsed - timeLast) < stageDelay[stage]) return;
    
    timeLast = elapsed;
    stage+=inc;

    if (stage>6 || stage<0 )
    {
      inc *= -1;
      stage+=inc*2;
    }
    
    if (stage==6) //randomly choose whether or not to go 'stuck'
      {
        randNumber = random(9);
        if (randNumber<5) { //set the last stage to 'stuck' 
          cols[6][0] = B01010000;
          cols[6][1] = B10101100;
          cols[6][2] = B01010000;
          cols[6][3] = B10101100;
          cols[6][4] = B01010000; 
        }
        else //reset the last stage to a solid color
        {
          cols[6][0] = B01010100;
          cols[6][1] = B10101000;
          cols[6][2] = B01010100;
          cols[6][3] = B10101000;
          cols[6][4] = B01010100;
        }
      }
     if (stage==0) //randomly choose whether or not to go 'stuck'
      {
        randNumber = random(9);
        if (randNumber<5) { //set the first stage to 'stuck' 
          cols[0][0] = B00101000; //R B R B R B
          cols[0][1] = B11010100; //B R B R B R
          cols[0][2] = B00101000; //R B R B R B
          cols[0][3] = B11010100; //B R B R B R
          cols[0][4] = B00101000; //R B R B R B
        }
        else //reset the first stage to a solid color
        {
          cols[0][0] = B10101000;
          cols[0][1] = B01010100;
          cols[0][2] = B10101000;
          cols[0][3] = B01010100;
          cols[0][4] = B10101000;
        }
      }

    for (int row=0; row<5; row++)
      control.setRow(device,row,cols[stage][row]);
  }
};
PSI psiFront=PSI(psiRed, psiBlue, rbSlide, 0);


void setup() {      
      
      delay(50); // sanity check delay
      randomSeed(analogRead(0)); //helps keep random numbers more random  
      #if (DEBUG>0)
      Serial.begin(9600);         
      #endif     
     
      lcPSI.shutdown(0, false); //take the device out of shutdown (power save) mode
      lcPSI.clearDisplay(0); 
      lcPSI.setIntensity(0,PSIbright);
      
      pinMode(TOGGLEPIN, INPUT);
      //digitalWrite(TOGGLEPIN, HIGH); //used for dipswitch prototype
      //logic=digitalRead(TOGGLEPIN);   
   logic=1; //sets this to RLD regardless of jumper   
      if (logic==1) {
        #if (DEBUG>1)
        Serial.println("RLD");
        #endif        
        //logic=1;
        //numLeds=96;
        //slow default speeds down for the RLD... 
        tweenPause=40;
        keyPause=1200;
        Keys=sizeof(rldColors)/3;
        #if (DEBUG>1)
        Serial.println(String(Keys)+" Keys");
        #endif
      }  
      else {
        #if (DEBUG>1)
        Serial.println("FLD");
        #endif
        //numLeds=80; 
        Keys=sizeof(fldColors)/3;        
      }     
  
      totalColors=Keys*Tweens;
      #if (DEBUG>1)
      Serial.println(String(Keys)+" Keys\n"+String(Tweens)+" Tweens\n"+String(totalColors)+" Total");
      #endif
      
      pinMode(SPEEDPIN, INPUT); 
      //digitalWrite(SPEEDPIN, HIGH); //used for dipswitch prototype
      if (digitalRead(SPEEDPIN)==HIGH) {
        speeds=1;  
        #if (DEBUG>1)
        Serial.println("SPD");
        #endif
      }  
      
      //make a giant array of all colors tweened...
      byte el,val,kcol,perStep,totalColorCount; //
      for(kcol=0;kcol<(Keys);kcol++) { //go through each Key color
        for(el=0;el<3;el++) { //loop through H, S and V 
          if (logic==0) perStep=int(pgm_read_byte(&fldColors[kcol+1][el])-pgm_read_byte(&fldColors[kcol][el]))/(Tweens+1);
          else perStep=int(pgm_read_byte(&rldColors[kcol+1][el])-pgm_read_byte(&rldColors[kcol][el]))/(Tweens+1);
          byte tweenCount=0;
          if (logic==0) val=pgm_read_byte(&fldColors[kcol][el]);
          else val=pgm_read_byte(&rldColors[kcol][el]);
          while (tweenCount<=Tweens) {
            if (tweenCount==0) totalColorCount=kcol*(Tweens+1); //this is the first transition, between these 2 colors, make sure the total count is right
            AllColors[totalColorCount][el]=val; //set the actual color element (H, S or V)
            tweenCount++;
            totalColorCount++;
            val=byte(val+perStep);
          }  
        }  
      }
      /*#if (DEBUG>1)
      // print all the colors
      for(byte x=0;x<totalColors;x++) {
        Serial.println(String(x)+" : "+String(AllColors[x][0])+","+String(AllColors[x][1])+","+String(AllColors[x][2])+"  o  "+String(oColors[x][0])+","+String(oColors[x][1])+","+String(oColors[x][2]));
      }  
      #endif */ 
      
      FastLED.setBrightness(maxBrightness); //sets the overall brightness to the maximum
      FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, 96);
      
      for(byte x=0;x<96;x++) leds[x] = CRGB::Black; //sets all possible LEDs to black
      FastLED.show();  
      
      //to get the RLD test mode, S4 should have jumper, S3 no jumper, Delay trimmer should be turned completely counter-clockwise
      int delayVal = analogRead(keyPin);      
      if (logic==1 && speeds==0 && delayVal<10) {
        //testpattern for RLD
        #if PCBVERSION<2
        byte testColor[96] = {
        1,1,1,1,1,1,1,1,
        2,3,3,3,3,2,2,2,
        4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,
        2,2,2,3,3,3,3,2,
        1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,
        2,2,2,3,3,3,3,2,
        4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,
        2,3,3,3,3,2,2,2,
        1,1,1,1,1,1,1,1};
        #else
        byte testColor[96] = {
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        2,2,2,3,3,3,3,2,2,3,3,3,3,2,2,2,
        4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        2,2,2,3,3,3,3,2,2,3,3,3,3,2,2,2,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};   
        #endif
        //go through all our LEDs, setting them to appropriate colors
        for(byte x=0;x<96;x++) {
           if (testColor[x]==1) leds[x] = CRGB::DarkRed;
           if (testColor[x]==2) leds[x] = CRGB::DarkOrange;
           if (testColor[x]==3) leds[x] = CRGB::DarkGreen;
           if (testColor[x]==4) leds[x] = CRGB::DeepSkyBlue;
           if (testColor[x]==5) leds[x] = CRGB::Orchid;
           FastLED.setBrightness(10);  
           FastLED.show();  
           delay(10);        
        }
        delay(999999);
        FastLED.setBrightness(maxBrightness);
      }
      
      //configure each LED's status (color number, direction, pause time)
      for(byte x=0;x<96;x++) {
        LEDstat[x][0]=byte(random(totalColors)); //choose a random color number to start this LED at
        LEDstat[x][1]=random(2); //choose a random direction for this LED (0 up or 1 down)
        if (LEDstat[x][0]%(Tweens+1)==0) LEDstat[x][2]=random(keyPause); //color is a key, set its pause time for longer than tweens
        else LEDstat[x][2]=random(tweenPause);
      }
      //now set the LEDs to their initial colors...
      for(byte x=0;x<96;x++) {          
        if (logic==1)  leds[pgm_read_byte(&rldMap[x])].setHSV(AllColors[LEDstat[x][0]][0],AllColors[LEDstat[x][0]][1],AllColors[LEDstat[x][0]][2]); 
        else {
          if (x<80) leds[pgm_read_byte(&fldMap[x])].setHSV(AllColors[LEDstat[x][0]][0],AllColors[LEDstat[x][0]][1],AllColors[LEDstat[x][0]][2]) ; 
        }  
        FastLED.show(); 
        delay(10);
      }
  
      //do a startup animation of some sort
      for(byte x=0;x<96;x++) {
        if (logic==1) {
          //RLD STARTUP
          leds[pgm_read_byte(&rldMap[x])] = CRGB::Green;
          if ((x+1)%24==0) { delay(75); FastLED.show(); }
        }  
        else {
          //FLD STARTUP
          if (x<80) {
            leds[pgm_read_byte(&fldMap[x])] = CRGB::Blue;
            if ((x+1)%8==0) { delay(50); FastLED.show(); } 
          }         
        } 
             
      }  
      
      #if (DEBUG>0)
      Serial.println(memoryFree()); // print the free memory 
      delay(250); 
      #endif
}

#if (DEBUG>0)
// variables created by the build process when compiling the sketch (used for the memoryFree function)
extern int __bss_end;
extern void *__brkval;
// function to return the amount of free RAM
int memoryFree() {
  int freeValue;
  if((int)__brkval == 0) freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}
#endif


void updateLED(byte LEDnum, byte hueVal) {
    //this will take an LED number and adjust its status in the LEDstat array
    //check the current color this LED is set to...
    //unsigned int currentColor=LEDstat[LEDnum];  
    if (LEDstat[LEDnum][2]!=0) {
      //LED is paused
      LEDstat[LEDnum][2]=LEDstat[LEDnum][2]-1; //reduce the LEDs pause number and check back next loop
    }
    else {
        //LED had 0 pause time, let's change things around...
        if (LEDstat[LEDnum][1]==0 && LEDstat[LEDnum][0]<(totalColors-1)) {
            LEDstat[LEDnum][0]=LEDstat[LEDnum][0]+1; //change it to next color
            leds[LEDnum].setHSV(AllColors[LEDstat[LEDnum][0]][0]+hueVal,AllColors[LEDstat[LEDnum][0]][1],AllColors[LEDstat[LEDnum][0]][2]);
            if (LEDstat[LEDnum][0]%(Keys+1)==0) LEDstat[LEDnum][2]=random(keyPause); //color is a key, set its pause time for longer than tweens
            else LEDstat[LEDnum][2]=random(tweenPause);
        }
        else if (LEDstat[LEDnum][1]==0 && LEDstat[LEDnum][0]==(totalColors-1)) {
            LEDstat[LEDnum][1]=1; //LED is at the final color, leave color but change direction to down
        }
        else if (LEDstat[LEDnum][1]==1 && LEDstat[LEDnum][0]>0) {
            LEDstat[LEDnum][0]=LEDstat[LEDnum][0]-1; //change it to previous color
            leds[LEDnum].setHSV(AllColors[LEDstat[LEDnum][0]][0]+hueVal,AllColors[LEDstat[LEDnum][0]][1],AllColors[LEDstat[LEDnum][0]][2]);
            if (LEDstat[LEDnum][0]%(Keys+1)==0) {
              LEDstat[LEDnum][2]=random(keyPause); //new color is a key, set LED's pause time for longer than tweens
            }
            else LEDstat[LEDnum][2]=tweenPause;
        }
        else if (LEDstat[LEDnum][1]==1 && LEDstat[LEDnum][0]==0) {
            LEDstat[LEDnum][1]=0; //LED is at the first color (0), leave color but change direction to up
        }
    }   
}


void loop() {
  
    /*#if (DEBUG>1)
    if (loopCount==1) time=millis();
    #endif*/
  
    if (loopCount==LOOPCHECK) { //only check this stuff every 100 or so loops
       
       #if (DEBUG>0) 
       Serial.println(String(memoryFree())); // print the free memory  
       #endif
       #if (DEBUG>1)
       time=(micros()-time)/(LOOPCHECK-1);
       Serial.println("lps "+String(1000000/time)+"\n");
       time=micros();
       #endif
              
       if (speeds==1) {
         /*#if (DEBUG>1)
         Serial.println("checking key and tween pots\n"); 
         #endif*/
         keyPause = analogRead(keyPin);  
         tweenPause = round(analogRead(tweenPin)/10);
       }
       
       // LET THE USER ADJUST GLOBAL BRIGHTNESS... 
       briVal = (round(analogRead(briPin)/4)*maxBrightness)/255; //the Bright trimpot has a value between 0 and 1024, we divide this down to between 0 and our maxBrightness
       if (briVal!=prevBri) {
         briDiff=(max(briVal,prevBri)-min(briVal,prevBri)); 
         if (briDiff>=2) {
             /*#if (DEBUG>1)
             Serial.println("bri "+String(briVal)); 
             //delay(100);
             #endif*/
             FastLED.setBrightness(briVal); //sets the overall brightness
         }
         prevBri=briVal;
       } 
       loopCount=0;       
    }
    
    unsigned long timeNew= millis();
    psiFront.Animate(timeNew, lcPSI);
    
    loopCount++;    
    
    hueVal = round(analogRead(huePin)/4); //read the Color trimpot (gets passed to updateLED for each LED)
    //go through each LED and update it 
    for(byte LEDnum=0;LEDnum<96;LEDnum++) {
      if (logic==1) updateLED(pgm_read_byte(&rldMap[LEDnum]),hueVal);
      else if (LEDnum<80) updateLED(pgm_read_byte(&fldMap[LEDnum]),hueVal); 
    }  
    FastLED.show();
    
    #if (DEBUG>2)
    delay(10); //slow things down to a crawl
    #endif
}


