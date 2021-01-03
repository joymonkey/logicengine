
byte rgbColor[3];
void hsv2rgb(uint8_t hue, uint8_t sat, uint8_t val) {
  uint16_t h = hue * 3;
  uint8_t r, g, b;
  //uint8_t value = dim_curve[ val ]; //dim_curve can be found in WS2812.h if necessary
  //uint8_t invsat = dim_curve[ 255 - sat ];
  uint8_t value = val;
  uint8_t invsat = 255 - sat;
  uint8_t brightness_floor = (value * invsat) / 256;
  uint8_t color_amplitude = value - brightness_floor;
  uint8_t section = h >> 8; // / HSV_SECTION_3; // 0..2
  uint8_t offset = h & 0xFF ; // % HSV_SECTION_3;  // 0..255
  uint8_t rampup = offset; // 0..255
  uint8_t rampdown = 255 - offset; // 255..0
  uint8_t rampup_amp_adj   = (rampup   * color_amplitude) / (256);
  uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256);
  uint8_t rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
  uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;
  if ( section ) {
    if ( section == 1) {
      // section 1: 0x40..0x7F
      r = brightness_floor;
      g = rampdown_adj_with_floor;
      b = rampup_adj_with_floor;
    }
    else {
      // section 2; 0x80..0xBF
      r = rampup_adj_with_floor;
      g = brightness_floor;
      b = rampdown_adj_with_floor;
    }
  }
  else {
    // section 0: 0x00..0x3F
    r = rampdown_adj_with_floor;
    g = rampup_adj_with_floor;
    b = brightness_floor;
  }
  rgbColor[0] = r;
  rgbColor[1] = g;
  rgbColor[2] = b;
}

byte hsvColor[3];
void calcColor(byte palNum, int colorNum) {
  if (colorNum % (TWEENS + 1) == 0) {
    //colorNum is a key, life is easy
    for (byte x = 0; x < 3; x++) {
      hsvColor[x] = keyColors[palNum][(colorNum / TWEENS)][x];
    }
  }
  else {
    //this color is a tween between two keys, calculate its H, S and V values. Oh the humanity!
    byte upperKey = ceil(colorNum / (TWEENS + 1) + 1);
    byte tweenNumDiff = colorNum - ((upperKey - 1) * (TWEENS + 1));
    for (byte x = 0; x < 3; x++) {
      hsvColor[x] = ( keyColors[palNum][upperKey - 1][x] + (tweenNumDiff *  (keyColors[palNum][upperKey][x] - keyColors[palNum][upperKey - 1][x]) / (TWEENS + 1)  ));
    }
  }
}

void calcColors(byte palNum, byte logicNum) {
  #if (DEBUG>0)
    if (logicNum == 0) {
      DEBUG_SERIAL.print(F("Front"));
      DEBUG_SERIAL.print(F(" Colors : hue+"));
      DEBUG_SERIAL.print(mySettings.frontHue);
      DEBUG_SERIAL.print(F("  desat"));
      DEBUG_SERIAL.print(mySettings.frontDesat);
      DEBUG_SERIAL.print(F("  bri"));
      DEBUG_SERIAL.println(byte(float(mySettings.maxBri) / 255 * mySettings.frontBri));
    }
    else {
      DEBUG_SERIAL.print(F("Rear"));
      DEBUG_SERIAL.print(F(" Colors : hue+"));
      DEBUG_SERIAL.print(mySettings.rearHue);
      DEBUG_SERIAL.print(F("  desat"));
      DEBUG_SERIAL.print(mySettings.rearDesat);
      DEBUG_SERIAL.print(F("  bri"));
      DEBUG_SERIAL.println(byte(float(mySettings.maxBri) / 255 * mySettings.rearBri));
    }
         
  #endif
  for (byte col = 0; col < TOTALCOLORS; col++) {
    calcColor(palNum, col);
    //now hsvColor contains our color
    //scale down the Val based on our brightness settings
    //say maxBri setting is 200/255 and frontBri setting is 50/255 , our value will be scaled to 200/255*50
    if (logicNum == 0) {      
      hsvColor[0] = hsvColor[0] + mySettings.frontHue;
      if (mySettings.frontDesat>0) hsvColor[1] = map(hsvColor[1],0,255,0,mySettings.frontDesat); //adjust for our stored desaturation value
      hsvColor[2] = map(hsvColor[2], 0, 255, 0, byte(float(mySettings.maxBri) / 255 * mySettings.frontBri) );
    }
    else {
      hsvColor[0] = hsvColor[0] + mySettings.rearHue;
      //if (mySettings.rearDesat>0) hsvColor[1] = map(hsvColor[1],0,255,0,mySettings.rearDesat); //adjust for our stored desaturation value
      //if (mySettings.rearDesat>0 && hsvColor[1]-mySettings.rearDesat>0) hsvColor[1] = hsvColor[1]-mySettings.rearDesat;
      if (mySettings.rearDesat>0) {
        #if (DEBUG>0)
        DEBUG_SERIAL.print(F("S was "));
        DEBUG_SERIAL.print(hsvColor[1]);
        DEBUG_SERIAL.print(F(" now "));
        #endif
        hsvColor[1] = map(hsvColor[1],0,255,0,mySettings.rearDesat); // 3K500x0D
        #if (DEBUG>0)
        DEBUG_SERIAL.println(hsvColor[1]);
        #endif
      }
      hsvColor[2] = map(hsvColor[2], 0, 255, 0, byte(float(mySettings.maxBri) / 255 * mySettings.rearBri) );
    }
    //now lets throw it into the allColors array in RGB format
    hsv2rgb(hsvColor[0], hsvColor[1], hsvColor[2]);
    for (byte x = 0; x < 3; x++) {
      allColors[logicNum][col][x] = rgbColor[x];
    }
  }
}

void checkTrimpots(bool startTrim = 0) {
  //check the current trimpot values and put them into startTrimpots[] or loopTrimpots[]
  if (startTrim == 0) {
    loopTrimpots[0] = map(analogRead(delayPin), 0, 1023, MIN_DELAY, MAX_DELAY);
    loopTrimpots[1] = map(analogRead(fadePin), 0, 1023, 0, MAX_FADE);
    loopTrimpots[2] = map(analogRead(briPin), 0, 1023, MIN_BRI, mySettings.maxBri);
    loopTrimpots[3] = map(analogRead(huePin), 0, 1023, 0, 255);
  }
  else {
    startTrimpots[0] = map(analogRead(delayPin), 0, 1023, MIN_DELAY, MAX_DELAY);
    startTrimpots[1] = map(analogRead(fadePin), 0, 1023, 0, MAX_FADE);
    startTrimpots[2] = map(analogRead(briPin), 0, 1023, MIN_BRI, mySettings.maxBri);
    startTrimpots[3] = map(analogRead(huePin), 0, 1023, 0, 255);
  }
}

void compareTrimpots(byte adjMode = 0) {
  checkTrimpots();
  for (byte x = 0; x < 4; x++) {
    if ( x > 1 && adjEnabled[x] == 0 && ( startTrimpots[x] - loopTrimpots[x] > adjThreshold || loopTrimpots[x] - startTrimpots[x] > adjThreshold )  ) { //compare Brightness and Hue using adjThreshold, as changes there can be a lot of work
      adjEnabled[x] = 1;
    }
    else if ( adjEnabled[x] == 0 && startTrimpots[x] != loopTrimpots[x] ) {
      adjEnabled[x] = 1;
      #if (DEBUG>0)
        DEBUG_SERIAL.print(x);
        DEBUG_SERIAL.println(F("ENABLED"));
      #endif
    }
    else if ( adjEnabled[x] == 1) {
      //if (loopTrimpots[x] != startTrimpots[x]) {
      if ((x==1 && loopTrimpots[x] != startTrimpots[x]) || (loopTrimpots[x]-startTrimpots[x]>=2 || startTrimpots[x]-loopTrimpots[x]>=2)) {
        #if (DEBUG>0)
          DEBUG_SERIAL.print(x);
          DEBUG_SERIAL.print("=");
          DEBUG_SERIAL.println(loopTrimpots[x]);
        #endif
        //adjustment is enabled for this pot, if settings have changed see if we need to recalc colors and all that jazz
        if (adjMode == 1) {
            //FRONT ADJUSTMENTS...
            if (x == 0) mySettings.frontDelay = loopTrimpots[x];
            else if (x == 1) mySettings.frontFade = loopTrimpots[x];
            else if (x == 2) {
              //map(hsvColor[2],0,255,0,byte(float(mySettings.maxBri)/255*mySettings.frontBri) )
              //mySettings.frontBri = map(loopTrimpots[x], 0, 1023, 0, 255); //if loopTrimpots were int's
              mySettings.frontBri = loopTrimpots[x];
              calcColors(mySettings.frontPalNum, 0);
            }
            else if (x == 3) {
              //mySettings.frontHue = map(loopTrimpots[x], 0, 1023, 0, 255); //if loopTrimpots were int's
              mySettings.frontHue = loopTrimpots[x];
              calcColors(mySettings.frontPalNum, 0);
            }
        }
        if (adjMode == 3) {
            if (x == 0) mySettings.rearDelay = loopTrimpots[x];
            else if (x == 1) mySettings.rearFade = loopTrimpots[x];
            else if (x == 2) {
              //map(hsvColor[2],0,255,0,byte(float(mySettings.maxBri)/255*mySettings.frontBri) )
              //mySettings.frontBri = map(loopTrimpots[x], 0, 1023, 0, 255); //if loopTrimpots were int's
              mySettings.rearBri = loopTrimpots[x];
              calcColors(mySettings.rearPalNum, 1);
            }
            else if (x == 3) {
              //mySettings.frontHue = map(loopTrimpots[x], 0, 1023, 0, 255); //if loopTrimpots were int's
              mySettings.rearHue = loopTrimpots[x];
              calcColors(mySettings.rearPalNum, 1);
            }
        }
      }
      //save the values for the next loop
      startTrimpots[x] = loopTrimpots[x];
    }
  }
}

// WHY DID I PUT A delay() IN THIS FUNCTION???!! SEE IF IT'S ACTUALLY USED...
void statusBlink(byte blinks, byte delayTime, byte redVal, byte greenVal, byte blueVal) {
  for ( byte x = 0; x <= blinks; x++) {
	  statusLED.setPixelColor(0, redVal, greenVal, blueVal);
	  statusLED.show();      
      delay(delayTime);
	  statusLED.setPixelColor(0, 0, 0, 0); 
	  statusLED.show();  
      delay(delayTime);
  }    
}


void saveSettings() {
  my_flash_store.write(mySettings); //////////////////////////////////////////////////////////////// TODO: make compatible with standard Arduino EEPROM
}


/*void checkAdjSwitch() {
  if (digitalRead(FADJ_PIN) == 0 && prevAdjMode != 1 && startAdjMode == 0) {
    adjMode = 1;
    checkTrimpots(1); //put initial trimpot values into startTrimpots[]
#if (DEBUG>0)
    DEBUG_SERIAL.println(F("adj Front"));
#endif
    //adjMillis = millis();
    adjLoops=0;
    statusFlipFlopTime = fastBlink;
  }
  else if (digitalRead(RADJ_PIN) == 0 && prevAdjMode != 3 && startAdjMode == 0) {
    adjMode = 3;
    checkTrimpots(1); //put initial trimpot values into startTrimpots[]
#if (DEBUG>0)
    DEBUG_SERIAL.println(F("adj Rear"));
#endif
    //adjMillis = millis();
    adjLoops=0;
    statusFlipFlopTime = fastBlink;
  }
  else if ( (prevAdjMode != 0 && digitalRead(RADJ_PIN) == 1 && digitalRead(FADJ_PIN) == 1 && startAdjMode == 0) || (adjLoops>adjLoopMax) ) {
      #if (DEBUG>0)
        if (adjLoops>adjLoopMax) DEBUG_SERIAL.println(F("MAXED OUT")); 
      #endif
      //if we were in previous adjMode for way too long, save settings here  SAVE STUFF HERE and go back to regular mode!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      if (adjLoops > adjLoopMin)  {
        #if (DEBUG>0)
          DEBUG_SERIAL.println(F("save"));
          DEBUG_SERIAL.print(F("FDelay "));
          DEBUG_SERIAL.println(mySettings.frontDelay);
          DEBUG_SERIAL.print(F("FFade "));
          DEBUG_SERIAL.println(mySettings.frontFade);
          DEBUG_SERIAL.print(F("FHue "));
          DEBUG_SERIAL.println(mySettings.frontHue);
          DEBUG_SERIAL.print(F("FBri "));
          DEBUG_SERIAL.println(mySettings.frontBri);
          DEBUG_SERIAL.print(F("FPal "));
          DEBUG_SERIAL.println(mySettings.frontPalNum);        
        #endif
        mySettings.writes++;
        saveSettings();
        if (adjLoops>adjLoopMax) {
          startAdjMode=adjMode;
          adjLoops=0;
        }
        adjMode = 0;
        for (byte x = 0; x < 4; x++) adjEnabled[x] = 0; //
        statusFlipFlopTime = slowBlink;            
      }
  }
  else if (digitalRead(RADJ_PIN) == 1 && digitalRead(FADJ_PIN) == 1 && startAdjMode != 0) {
    //adjMode didn't start off centered, which could have messed us up.
    //now it is centered though, so let's get back to our normal state.
    startAdjMode = 0;
  }
  if (adjMode != prevAdjMode) {
    statusBlink(2, 250, 1, 0, 2); //blink purple 2 times
  }
  prevAdjMode = adjMode;
}
*/

int checkPalButton() {
  if (digitalRead(PAL_PIN) == 0) {
    //button is held
    palPinLoops++;
    if (palPinStatus == 1 && prevPalPinStatus == 1) {
      //we just started holding the button
      palPinStatus = 0;
      palPinLoops=0;
    }
    return (0);
  }  
  else if (digitalRead(PAL_PIN) == 1 && palPinStatus == 0 && prevPalPinStatus == 0) {
  //else if (digitalRead(PAL_PIN) == 1 && prevPalPinStatus == 0) {
    //button has just been released
    palPinLoops++;
    palPinStatus = 1;
    return (palPinLoops);
  }
  prevPalPinStatus = palPinStatus;
}

//function takes a color number (that may be bizarro) and returns an actual color number
byte actualColorNum(byte x) {
  if (x >= TOTALCOLORS) x = (TOTALCOLORS - 2) - (x - TOTALCOLORS);
  return (x);
}

void setStatusLED() {
    if (statusFlipFlop == 0) {
      if (adjMode == 0) statusLED.setPixelColor(0, 0, 0, 2); //blue
      else if (adjMode == 1) statusLED.setPixelColor(0, 0, 0, 2); //blue
      else if (adjMode == 3) statusLED.setPixelColor(0, 0, 2, 0); //green
    }
    else {
      if (adjMode == 0) statusLED.setPixelColor(0, 2, 0, 0); //red
      else if (adjMode == 1) statusLED.setPixelColor(0, 2, 2, 2); //white
      else if (adjMode == 3) statusLED.setPixelColor(0, 2, 2, 0); //orangey

    }
}

void loadSettings(byte deviceNum = 0) {
  //load stored settings for specified logic display(s) (0 for all, 1 for front, 3 for rear)
  if (deviceNum==0) {
    mySettings = my_flash_store.read();                  //////////////////////////////////////////////////////////////// TODO: make compatible with standard Arduino EEPROM
  }
  else {
    tempSettings = my_flash_store.read();
  }
  if (deviceNum==1) {
    //just read front specific settings from stored values
    mySettings.frontDelay=tempSettings.frontDelay;
    mySettings.frontFade=tempSettings.frontFade;
    mySettings.frontBri=tempSettings.frontBri;
    mySettings.frontHue=tempSettings.frontHue;
    mySettings.frontPalNum=tempSettings.frontPalNum;
    mySettings.frontDesat=tempSettings.frontDesat;
    mySettings.frontScrollSpeed=tempSettings.frontScrollSpeed;
  }
  else if (deviceNum==3) {
    //just read rear specific settings from stored values
    mySettings.rearDelay=tempSettings.rearDelay;
    mySettings.rearFade=tempSettings.rearFade;
    mySettings.rearBri=tempSettings.rearBri;
    mySettings.rearHue=tempSettings.rearHue;
    mySettings.rearPalNum=tempSettings.rearPalNum;
    mySettings.rearDesat=tempSettings.rearDesat;
    mySettings.rearScrollSpeed=tempSettings.rearScrollSpeed;
  }
  if (deviceNum==0||deviceNum==1) calcColors(mySettings.frontPalNum, 0);
  if (deviceNum==0||deviceNum==3) calcColors(mySettings.rearPalNum, 1);  
}

void factorySettings() {
  mySettings = { (mySettings.writes+1), MAX_BRIGHTNESS,
                   DFLT_FRONT_DELAY, DFLT_FRONT_FADE, DFLT_FRONT_BRI, DFLT_FRONT_HUE, DFLT_FRONT_PAL, DFLT_FRONT_DESAT,
                   DFLT_REAR_DELAY,  DFLT_REAR_FADE,  DFLT_REAR_BRI,  DFLT_REAR_HUE,  DFLT_REAR_PAL, DFLT_REAR_DESAT,
                   DFLT_FRONT_SCROLL,DFLT_REAR_SCROLL
                 };
  calcColors(mySettings.frontPalNum, 0);
  calcColors(mySettings.rearPalNum, 1);                     
}

void changePalNum(byte logicAddress, byte palNum=0) {
    if (logicAddress == 1)   {
      if (palNum==0) {
        //new pal num wasnt specified, so cycle pals up
        mySettings.frontPalNum++;
        if (mySettings.frontPalNum == NUM_PALS) mySettings.frontPalNum = 0;
      }
      else mySettings.frontPalNum=palNum-1;
      //generate new front palette here!!!
      calcColors(mySettings.frontPalNum, 0);
      #if (DEBUG>0)
          DEBUG_SERIAL.print(F("pal"));
          DEBUG_SERIAL.println(mySettings.frontPalNum);
      #endif
    }
    else if (logicAddress == 3) {
      if (palNum==0) {
        mySettings.rearPalNum++;
        if (mySettings.rearPalNum == NUM_PALS) mySettings.rearPalNum = 0;
      }
      else mySettings.rearPalNum=palNum-1;
      //generate new rear palette here!!!
      calcColors(mySettings.rearPalNum, 1);
      #if (DEBUG>0)
          DEBUG_SERIAL.print(F("pal"));
          DEBUG_SERIAL.println(mySettings.rearPalNum);
      #endif
    }
}

byte buildCommand(char ch, char* output_str) {
  static uint8_t pos=0;
  if (ch=='\r') {  // end character recognized
      output_str[pos]='\0';   // append the end of string character
      pos=0;        // reset buffer pointer
      //doingSerialStuff=0;
      return true;      // return and signal command ready      
  }
  else {  // regular character
      output_str[pos]=ch;   // append the  character to the command string
      if(pos<=CMD_MAX_LENGTH-1)pos++; // too many characters, discard them.
  }
  return false;
}

void parseCommand(char* inputStr) {
        #if (DEBUG>0)
          DEBUG_SERIAL.print(F("parse "));
          DEBUG_SERIAL.println(inputStr);
        #endif
        byte hasArgument=false;
        int argument=0;
        byte deviceNum;
        byte pos=0;
        byte length=strlen(inputStr);
        if(length<2) goto beep;   // not enough characters

        // get the Device Number (aka address), one or two digits
        char addrStr[3];
        if(!isdigit(inputStr[pos])) goto beep;  // invalid, first char not a digit
        addrStr[pos]=inputStr[pos];
        pos++;                              // pos=1
        if(isdigit(inputStr[pos])) {        // add second digit address if it's there 
          if (addrStr[0]!=0) addrStr[pos]=inputStr[pos]; 
          pos++;                            // pos=2
        }
        addrStr[pos]='\0';                  // add null terminator
        deviceNum= atoi(addrStr);        // extract the address
        #if (DEBUG>0)   
          DEBUG_SERIAL.print(F("dev:"));
          DEBUG_SERIAL.println(deviceNum);
        #endif

        // check for more
        if(!length>pos) goto beep;            // invalid, no command after address

        //determine the argument value
        for(byte i=pos+1; i<length; i++) {
          if(!isdigit(inputStr[i])&inputStr[pos+1]!='r'&&inputStr[pos]!='M') {
            #if (DEBUG>0)   
              DEBUG_SERIAL.print(inputStr[i]);
              DEBUG_SERIAL.println(F(" invalid"));
            #endif
            goto beep; // invalid, end of string contains non-numerial arguments
          }
        } 
        argument=atoi(inputStr+pos+1);    // that's the numerical argument after the command character
        #if (DEBUG>0)   
          DEBUG_SERIAL.print(F("arg:"));
          DEBUG_SERIAL.println(argument);
        #endif
        
       
        //check for a valid command letter. F (fade),J (delay),H (hue),J (bri) or P (parameter)
        if (inputStr[pos]=='P') {
          #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("P command"));
          #endif
          // valid P options are 70 (Reset), 71 (Load) , 72 (Save), 73 (Palette)
            if (argument==73) {
              // ** command P73 , cycle palette(s)
              if (deviceNum==1||deviceNum==0) changePalNum(1);
              if (deviceNum==3||deviceNum==0) changePalNum(3);
            }
            else if (argument==70) {
              // ** command P70 , load factory settings
              factorySettings();
            }
            else if (argument==71) {
              // ** command P71 , load stored settings
              if (deviceNum==0)loadSettings();
              else if (deviceNum==1)loadSettings(1);
              else if (deviceNum==3)loadSettings(3);
            }          
        }
        else if (inputStr[pos]=='T') {
          #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("T command"));
          #endif
          if (argument==2||argument==3||argument==5) {
            //alarm mode, changes up the pallettes
            #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("Alarm"));
            #endif
            mySettings.frontHue=0;
            mySettings.rearHue=0;
            mySettings.frontDelay=0;
            mySettings.rearDelay=0;
            mySettings.frontFade=0;
            mySettings.rearFade=0;
            mySettings.frontBri=255;
            mySettings.rearBri=255;
            changePalNum(1, 3);
            changePalNum(3, 3);
            effectRunning=2;
            effectEndTime=currentMillis+2000; //run this effect for a while, then go back to stored settings
          }
          else if (argument==4) {
            //short circuit effect
            #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("ShrtCrkt"));
            #endif
          }
          else if (argument==6) {
            //Leia "Help Me" message
            #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("Leia"));
            #endif
            mySettings.frontHue=0;
            mySettings.rearHue=0;
            changePalNum(1, 1);
            changePalNum(3, 1);
            effectRunning=6;
            effectEndTime=currentMillis+10000; //run this effect for a while, then go back to stored settings
            tempSettings.frontBri=mySettings.frontBri;
            tempSettings.rearBri=mySettings.rearBri;
            mySettings.rearBri=mySettings.frontBri;
          }
          else if (argument==10) {
            //Star Wars
            #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("Star Wars"));
            #endif
            displayMode[1]=1;
            displayMode[2]=1;
            displayMode[3]=1; //0=blinkies, 1=scrollingtext
            strcpy(logicText[2]," STAR WARS ");
            strcpy(logicText[0],"STAR   ");
            strcpy(logicText[1],"   WARS");
            rearMatrix.setTextColor(rearMatrix.Color(120,100,0));
            frontMatrix.setTextColor(rearMatrix.Color(120,100,0));         
            mySettings.frontHue=37;
            mySettings.rearHue=37;
            changePalNum(1, 3);
            changePalNum(3, 3);
            effectRunning=10;
            effectEndTime=currentMillis+10000; //run this effect for a while, then go back to stored settings
          }
          else if (inputStr[pos+1]=='r') {
              #if (DEBUG>0)   
                DEBUG_SERIAL.println(F("reset"));
              #endif
              // ** Tr, same as command P71 , load stored settings but also enables all logics
              if (deviceNum==0){
                loadSettings();
                onOff[1]=1;
                onOff[2]=1;
                onOff[3]=1;
              }
              else if (deviceNum==1){
                loadSettings(1);
                onOff[1]=1;
                onOff[2]=1;
              }
              else if (deviceNum==3){
                loadSettings(3);
                onOff[3]=1;
              }
          }
        }
        else if (inputStr[pos]=='O') {
          #if (DEBUG>0)   
              DEBUG_SERIAL.println(F("O command"));
          #endif
          #if (DEBUG>0)   
            DEBUG_SERIAL.print(F("onoff"));
            DEBUG_SERIAL.println(inputStr[2]);
          #endif
          // ** command O changes a device's on/off state
          if (argument==0) {
            if (deviceNum==0||deviceNum==1) {
              onOff[1]=0;
              frontMatrix.fillScreen(0);
            }
            if (deviceNum==0||deviceNum==2) { 
              onOff[2]=0;
              frontMatrix.fillScreen(0);
            }
            if (deviceNum==0||deviceNum==3) {
              onOff[3]=0;
              rearMatrix.fillScreen(0);            
            }
          }
          else if (argument==1) {
            if (deviceNum==0||deviceNum==1) onOff[1]=1;
            if (deviceNum==0||inputStr[0]=='2') onOff[2]=1;
            if (deviceNum==0||deviceNum==3) onOff[3]=1;
          }
        }
        else if (inputStr[pos]=='M') {
            #if (DEBUG>0)   
              DEBUG_SERIAL.print(F("msg:"));
              DEBUG_SERIAL.println(inputStr+1+pos);
            #endif            
            for (int i = 0; inputStr[i]; i++ ) {
              if(isLowerCase(inputStr[i])) inputStr[i]=toUpperCase(inputStr[i]); //this converts each char to uppercase
            }
            #if (DEBUG>0)   
              DEBUG_SERIAL.print(F("msg:"));
              DEBUG_SERIAL.println(inputStr+1+pos);
            #endif   
            if (strlen(inputStr+1+pos)<2) goto beep;
            if (deviceNum==0||deviceNum==1) {
              for( int i = 0; i < sizeof(logicText[0]);  ++i ) logicText[0][i] = (char)0;
              strncpy(logicText[0], (inputStr+1+pos), strlen(inputStr+1+pos));
              displayMode[1]=1;
            }
            if (deviceNum==0||deviceNum==2) {
              for( int i = 0; i < sizeof(logicText[1]);  ++i ) logicText[1][i] = (char)0;
              strncpy(logicText[1], (inputStr+1+pos), strlen(inputStr+1+pos));
              displayMode[2]=1;
            }
            if (deviceNum==0||deviceNum==3) {
              for( int i = 0; i < sizeof(logicText[2]);  ++i ) logicText[2][i] = (char)0;
              strncpy(logicText[2], (inputStr+1+pos), strlen(inputStr+1+pos));
              displayMode[3]=1; 
            }
            #if (DEBUG>0)
              DEBUG_SERIAL.print(F("dspMode1:"));
              DEBUG_SERIAL.println(displayMode[1]);
              DEBUG_SERIAL.print(F("dspMode2:"));
              DEBUG_SERIAL.println(displayMode[2]);
              DEBUG_SERIAL.print(F("dspMode3:"));
              DEBUG_SERIAL.println(displayMode[3]);
            #endif  
        }
        //F (fade),G (delay),H (hue),J (bri),K (saturation)
        //these commands will have a value from 0-255 after the command letter
        if (inputStr[pos]=='F') {
          if (deviceNum==0) {
            mySettings.frontFade=argument;
            mySettings.rearFade=argument; 
          }
          else if (deviceNum==1) mySettings.frontFade=argument;
          else if (deviceNum==3) mySettings.rearFade=argument;             
        }
        else if (inputStr[pos]=='G') {
          if (deviceNum==0) {
            mySettings.frontDelay=argument;
            mySettings.rearDelay=argument; 
          }
          else if (deviceNum==1) mySettings.frontDelay=argument;
          else if (deviceNum==3) mySettings.rearDelay=argument;             
        }   
        else if (inputStr[pos]=='H') {
          if (deviceNum==0) {
            mySettings.frontHue=argument;
            calcColors(mySettings.frontPalNum, 0);
            mySettings.rearHue=argument; 
            calcColors(mySettings.rearPalNum, 1); 
          }
          else if (deviceNum==1) {
            mySettings.frontHue=argument;
            calcColors(mySettings.frontPalNum, 0);
          }
          else if (deviceNum==3) {
            mySettings.rearHue=argument; 
            calcColors(mySettings.rearPalNum, 1);            
          }
        } 
        else if (inputStr[pos]=='K') {
          //adjust Desaturation setting
          if (deviceNum==1 || deviceNum==0) {
            mySettings.frontDesat=argument;
            calcColors(mySettings.frontPalNum, 0);
          }
          if (deviceNum==3 || deviceNum==0) {
            mySettings.rearDesat=argument; 
            calcColors(mySettings.rearPalNum, 1);            
          }
        }
        else if (inputStr[pos]=='J') {
          if (deviceNum==0) {
            mySettings.frontBri=argument;
            calcColors(mySettings.frontPalNum, 0);
            mySettings.rearBri=argument; 
            calcColors(mySettings.rearPalNum, 1); 
          }
          else if (deviceNum==1) {
            mySettings.frontBri=argument;
            calcColors(mySettings.frontPalNum, 0);
          }
          else if (deviceNum==3) {
            mySettings.rearBri=argument; 
            calcColors(mySettings.rearPalNum, 1);            
          }
        } 
        

        return;                               // normal exit
  
        beep:                                 // error exit
        #if (DEBUG>0)
          DEBUG_SERIAL.println(F("beep"));
        #endif
        //JEDI_SERIAL.write(0x7);               // beep the terminal, if connected
        return;
}

void checkJediSerial() {
  if (JEDI_SERIAL.available() > 0) {
    doingSerialStuff=1; 
    serialMillis = millis();        
    ch=JEDI_SERIAL.read();  // get input
    //JEDI_SERIAL.print(ch);  // echo back
    #if (DEBUG>0)
      //DEBUG_SERIAL.println(ch);
      //DEBUG_SERIAL.println(ch,HEX);
    #endif
    command_available=buildCommand(ch, cmdString);  // build command line
    if (command_available) {
      parseCommand(cmdString);  // interpret the command
      //JEDI_SERIAL.print(F("\n> "));  // prompt again
      //DEBUG_SERIAL.print(F("\n> ")); // prompt again
    } 
  }
  #if (DEBUG>0)
    if (DEBUG_SERIAL.available() > 0) {
      doingSerialStuff=1; 
      serialMillis = millis();        
      ch=DEBUG_SERIAL.read();  // get input
      command_available=buildCommand(ch, cmdString);  // build command line
      if (command_available) {
        parseCommand(cmdString);  // interpret the command
      } 
    }
  #endif
}


#if (TEECESPSI>0)
   /*
     Each PSI has 7 states. For example on the front...
      0 = 0 columns Red, 6 columns Blue
      1 = 1 columns Red, 5 columns Blue (11)
      2 = 2 columns Red, 4 columns Blue (10)
      3 = 3 columns Red, 3 columns Blue  (9)
      4 = 4 columns Red, 2 columns Blue  (8)
      5 = 5 columns Red, 1 columns Blue  (7)
      6 = 6 columns Red, 0 columns Blue
  */
  void setPSIstate(bool frontRear, byte PSIstate) {
    //set PSI (0 or 1) to a state between 0 (full red) and 6 (full blue)
    // states 7 to 11 are moving backwards
    if (PSIstate > 6) PSIstate = 12 - PSIstate;
    for (byte col = 0; col < 6; col++) {
      if (col < PSIstate) {
        if (col % 2) lcChain.setColumn(frontRear, col, B10101010);
        else lcChain.setColumn(frontRear, col,      B01010101);
      }
      else {
        if (col % 2) lcChain.setColumn(frontRear, col, B01010101);
        else lcChain.setColumn(frontRear, col,      B10101010);
      }
    }
  }
#endif
