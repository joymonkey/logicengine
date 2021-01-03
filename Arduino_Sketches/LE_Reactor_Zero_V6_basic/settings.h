#define RLD112 true

#define BAUDRATE 2400 //this is the baudrate that we use to listen for incoming commands over JEDI_SERIAL
#define DEBUG_SERIAL SerialUSB
#define JEDI_SERIAL Serial1
#define DEBUG 0  //change to 1 to print some useful stuff to the serial monitor, 2 will also check loops-per-second
#define TEECESPSI 1

//default "factory" settings...
#define DFLT_FRONT_FADE 1 //was 3
#define DFLT_FRONT_DELAY 20 //was 60
#define DFLT_REAR_FADE 5 //was 10
#define DFLT_REAR_DELAY 200 //was 400
#define MAX_BRIGHTNESS 180
#define DFLT_FRONT_HUE 0
#define DFLT_REAR_HUE 0
#define DFLT_FRONT_PAL 0
#define DFLT_REAR_PAL 1
#define DFLT_FRONT_BRI 120
#define DFLT_REAR_BRI 100
#define DFLT_FRONT_DESAT 0
#define DFLT_REAR_DESAT 0
#define DFLT_FRONT_SCROLL 60
#define DFLT_REAR_SCROLL 40

#define MAX_FADE 15
#define MAX_DELAY 500
#define MIN_DELAY 10
#define MIN_BRI 10

#define TWEENS 8 // was 20

#define delayPin A0
#define fadePin A1
#define briPin A2
#define huePin A3
#define FADJ_PIN 2
#define RADJ_PIN 4
#define PAL_PIN 9
#define FRONT_PIN 5
#define REAR_PIN 3
#define REAR_DAT_PIN 11
#define REAR_CLK_PIN 3
#define STATUSLED_PIN 8

//we store the colour pallettes here as an array of colors in HSV (Hue,Saturation,Value) format
#define NUM_PALS 4
const byte keyColors[NUM_PALS][4][3] = {
  { {170, 255, 0} , {170, 255, 85} , {170, 255, 170} , {170, 0,170}  } , //front colors
  { {90, 235, 0}  , {75, 255, 250} , {30, 255, 184}  , {0, 255, 250}  } , //rear colors (hues: 87=bright green, 79=yellow green, 45=orangey yellow, 0=red)
  { {0, 255, 0}   , {0, 255, 0}   , {0, 255, 100}   , {0, 255, 250}  } , //monotone (black to solid red)
  { {0, 255, 0}   , {0, 255, 250}   , {40, 255, 0}   , {40, 255, 250}}  //dual color red and yellow
};

//Teeces PSI related settings...
#define TEECES_D_PIN 12
#define TEECES_C_PIN 10
#define TEECES_L_PIN 6
#define RPSIbright 15 //rear PSI
#define FPSIbright 15 //front PSI
#define PSIstuck 5 //odds (in 100) that a PSI will get partially stuck between 2 colors
  
