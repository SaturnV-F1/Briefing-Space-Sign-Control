/* Attempting to make a Brief in Progess Sign. No one should
   reuse this code because I'm sure its the least efficient, most brute force
   way possible of doing all these things. No seriously, I know nothing about
   computer engineering or software design. A trained monkey - or sufficiently
   many untrained monkeys in a room full of keyboards given enough time - could
   probably make better code.
*/

#include "RGBmatrixPanel.h"
#include <SPI.h> // why is this one different? Just to remind me you can use <>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "SdFat.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_ImageReader.h"
#include "TouchScreen.h"
#include "Fonts/FreeSerifBold18pt7b.h"
#include "Fonts/FreeSerifBold12pt7b.h"
#include "Fonts/FreeSerifBold9pt7b.h"
#include "Wire.h"
#include "time.h"
#include "DS3231M.h"

/* This section defines pins for the RGB LED matix:
   Most of the signal pins are configurable, but the CLK pin has some
   special constraints.  On 8-bit AVR boards it must be on PORTB...
   Pin 11 works on the Arduino Mega.  On 32-bit SAMD boards it must be
   on the same PORT as the RGB data pins (D2-D7)...
   Pin 8 works on the Adafruit Metro M0 or Arduino Zero,
   Pin A4 works on the Adafruit Metro M4 (if using the Adafruit RGB
   Matrix Shield, cut trace between CLK pads and run a wire to A4).
*/

#define CLK 11 // USE THIS ON ARDUINO MEGA
#define OE   9
#define LAT 10
#define A   A0
#define B   A1
#define C   A2
#define D   A3 // only used for 64 width matrix.

//RGB Matrix class call
RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);

//This section sets up pins for the TFT screen
#define TFT_CS 43 // reversed for image writting test
#define TFT_DC 45 // reversed for image writting test
#define TFT_RST 30 // RST can be set to -1 if you tie it to Arduino's reset

// This is to set the CCS line so the SD card gets chip selected
#define SD_CS 53

// These are the four touchscreen analog pins
#define YP A8   // must be an analog pin, use "An" notation!
#define XM A9   // must be an analog pin, use "An" notation!
#define YM 37   // can be a digital pin
#define XP 35   // can be a digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 110
#define TS_MINY 80
#define TS_MAXX 900
#define TS_MAXY 940

#define MINPRESSURE 10
#define MAXPRESSURE 1000

//UI Elements
#define HX8357_ORANGE 0xFA60
int BOX_WIDTH = 120;
int BOX_HEIGHT = 80;
int COL_SPACER = 20;
int ROW_SPACER = 14;

// Flags Struct for state machine. Is this really a state machine? Maybe.
struct {
  bool Start_Menu = false;
  bool Set_Clock_Menu = false;
  int Class_Menu = 2;
  int Begin_LED = 2;
  int Cancel_Disp = 2;
  int hold_count = 0;
  char Run_Mode = 'N';
  int Ready_to_run = 0;
  int Run_Mode_Menu = 2;
  int p_x_was = 0;
  int p_y_was = 0;
  unsigned long px_sum = 0;
  unsigned long py_sum = 0;
  unsigned long pz_sum = 0;
  unsigned long px_average = 0;
  unsigned long py_average = 0;
  unsigned long pz_average = 100;
} Flags;

//Classification Struct
struct {
int Red = 0;
int Green = 0;
int Blue = 0;
String str_class = "";
String str1 = "Brief In";
String str2 = "Progress";
} Brief_Class; // 

//Struct and address def needed for the DS3231M real time clock
struct tm timeinfo;             // timeinfo structure (defined in time.h)
#define addr_DS3231 0x68        // I2C address of the RTC module
DS3231M_Class DS3231M;
const uint8_t  SPRINTF_BUFFER_SIZE{32};  ///< Buffer size for sprintf()

/* For better pressure precision, we need to know the resistance
   between X+ and X- Use any multimeter to read it
   For the one we're using, its 300 ohms across the X plate */
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 250);

//Setup stuff for the SD card commands
  SdFat SD;
  Adafruit_ImageReader reader(SD);

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
  Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC);

/* This is the reset function. There is no special sginifigance 
 *  to it being above the setup function. It likely shouldn't
 *  re-call setup() but I can't think of a better way to do it.
 */
void do_nothing_but_reset(){
  Flags.Start_Menu = false;
  Flags.Set_Clock_Menu = false;
  Flags.Class_Menu = 2;
  Flags.Begin_LED = 2;
  Flags.Cancel_Disp = 2;
  Flags.hold_count = 0;
  Flags.Run_Mode = 'N';
  Flags.Ready_to_run = 0;
  Flags.Run_Mode_Menu = 2;
  Brief_Class.str_class = "";
  tft.setTextSize(1);
  tft.fillScreen(HX8357_BLACK);
  setup();
}

/*--------------------------------------SETUP------------------------------------------------//
//-----------------------------------**setup()**---------------------------------------------//
//-----------------Begins matrix, tft, serial, and SD card functions-------------------------*/

void setup() {
  matrix.begin();                      //Begin the class that drives the LED matrix
  matrix.fillScreen(matrix.Color333(0, 0, 0));
  Serial.begin(115200);                //Start serial communication for debugging
  tft.begin();                         // Start TFT functions

  tft.fillScreen(HX8357_BLACK);
  tft.setRotation(3);

  !SD.begin(SD_CS, SD_SCK_MHZ(25));    //Attempt to draw image from SD Card
  Serial.println(Flags.Start_Menu);
  //delay(1000);

  //Start I2C functions.  
  Wire.begin();           // enable I2C communications 
  //setupRtc();             // get the clock into a known state

  //Using the DS3231M clas functions for ease... until they stop working
  DS3231M.begin();
  DS3231M.pinSquareWave();
  DS3231M.adjust();
  
  char buf = 0;
}

/*--------------------------------------MAIN------------------------------------------------//
//-----------------------------------**loop()**---------------------------------------------//
//--------------------------------Does all the things---------------------------------------*/

void loop() {
  Serial.print("Current value of Run_Mode is: "); Serial.println(Flags.Run_Mode);
  Serial.print("Current value of First start flag is: "); Serial.println(Flags.Start_Menu);
  //delay(1000);
  if (Flags.Start_Menu == false) {
    First_Start_Menu();
  }

  if (Flags.Run_Mode == 'N') {
    Set_Run_Mode();
    Serial.print("At just above Run_Mode switch, Run_Mode flag is: ");Serial.println(Flags.Run_Mode);//delay(3000);
    switch (Flags.Run_Mode) {
      case 'T' :
        //If statement needs to go here to see if clock is set.
        Set_Clock();
        Serial.println("Set_Clock was called");
        break;
      case 'I' : // This is run indef mode, ignores the clock module
        break;
    }
  }
  if (Brief_Class.str_class == "") {
    Serial.println("Class_Select_Page was called");
    Class_Select_Page();
  }

  if (Brief_Class.str_class != "" && Flags.Run_Mode != 0) {
    Begin_Execution();
  }

  if (Flags.Ready_to_run == 1) {
    Set_LED_Screen();
    Serial.print("Set_LED_Screen has been called. str_class is set to: "); Serial.println(Brief_Class.str_class);
    //delay(5000);
    Cancel();
  }
}
/*-----------------------------------SUBROUTINE----------------------------------------------//
//--------------------------------**Set_LED_Screen*------------------------------------------//
//-----Sends information to the LED screen. Very resource heavy, only called at the end------*/

void Set_LED_Screen() {
  unsigned int start_p = (64 - (Brief_Class.str_class.length() * 6)) / 2;
  //Serial.print("str_class length is: "); Serial.println(Brief_Class.str_class.length());
  //matrix.setCursor(start_p, 0);
  matrix.drawRect(0, 0, matrix.width(), 14, matrix.Color333(Brief_Class.Red, Brief_Class.Green, Brief_Class.Blue));
  matrix.drawRect(1, 1, matrix.width()-2, 12, matrix.Color333(Brief_Class.Red, Brief_Class.Green, Brief_Class.Blue));
  matrix.setCursor(start_p, 3);
  matrix.setTextSize(1);     // size 1 == 8 pixels high
  matrix.setTextWrap(false);
  matrix.setTextColor(matrix.Color333(Brief_Class.Red, Brief_Class.Green, Brief_Class.Blue));
  //Serial.print("from within Set_LED_Screen, str_class is: ");Serial.println(Brief_Class.str_class);
  matrix.print(Brief_Class.str_class);

  unsigned int start_p1 = (64 - (Brief_Class.str1.length() * 6)) / 2;
  matrix.setTextColor(matrix.Color333(7, 7, 7));
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setCursor(start_p1, 15);
  matrix.print(Brief_Class.str1);

  unsigned int start_p2 = (64 - (Brief_Class.str2.length() * 6)) / 2;
  matrix.setTextColor(matrix.Color333(7, 7, 7));
  matrix.setTextSize(1);
  matrix.setTextWrap(false);
  matrix.setCursor(start_p2, 23);
  matrix.print(Brief_Class.str2);

}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//------------------------------**First_Start_Menu**-----------------------------------------//
//-----------------Draws UI elements for first menu when powered on--------------------------*/

void First_Start_Menu() {
  //Flags.Start_Menu = false;
  Serial.println("first start function was called");
  reader.drawBMP("/seal_only.bmp", tft, 32, (tft.height() - 230) / 2);
  tft.setTextColor(HX8357_WHITE);
  tft.setFont(&FreeSerifBold18pt7b);
  drawCentreString("Briefing Space Sign Control", tft.width(), 30); // font is 31 pix high
  drawCentreString("Touch Anywhere to Begin", tft.width(), tft.height() - 20); // font is 31 pix high
  
  while (Flags.Start_Menu == false) {

  TSPoint p = ts.getPoint();

  int x, y, x1, y1, w, h;
  static uint8_t secs, mins, hours;
  DateTime now = DS3231M.now();
  if (secs != now.second())// Output if seconds have changed
  {
    // Use sprintf() to pretty print the date/time with leading zeros
    char output_buffer_date[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    char output_buffer_time[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    sprintf(output_buffer_date, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    sprintf(output_buffer_time, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.setFont(&FreeSerifBold9pt7b);
    tft.setCursor(310, 95);  // font is 16 pix high
    tft.println(output_buffer_date);
    tft.setCursor(310, 115);  // font is 16 pix high
    tft.getTextBounds(output_buffer_time, x, y, &x1, &y1, &w, &h);
    tft.fillRect(358, 115-h, 20, h+2, HX8357_BLACK);
    
    //Draw over the minutes when the minutes change
    if (mins != now.minute()){
      tft.fillRect(333, 115-h, 19, h+2, HX8357_BLACK);
    } mins = now.minute();
    
    //Draw over the hours when the hours change
    if (hours != now.hour()){
      tft.fillRect(310, 115-h, 50, h+2, HX8357_BLACK);
    } hours = now.hour();

    tft.println(output_buffer_time);
    secs = now.second();
    //delay(3000);
  }

  Serial.println("stuck in the first start loop");
  Serial.print("\tPressure= "); Serial.println(p.z);

    int touch_count = 0;
    while (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      TSPoint p = ts.getPoint();
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
      Serial.print("P.z is: ");Serial.println(p.z);
      touch_count ++;
      
      Serial.print("touch_count is: "); Serial.println(touch_count);

      //Running average of p.x and p.y

      Flags.px_sum = Flags.px_sum + p.x;
      Flags.py_sum = Flags.py_sum + p.y;
      Flags.pz_sum = Flags.pz_sum + p.z;
      Flags.px_average = Flags.px_sum / touch_count;
      Flags.py_average = Flags.py_sum / touch_count;
      Flags.pz_average = Flags.pz_sum / touch_count;
      Serial.print("Averaged p.x and p.y values: ");Serial.print(Flags.px_average); Serial.print(" ");Serial.print(Flags.py_average); 
      
      if (touch_count > 100 & p.z > MINPRESSURE) {
        tft.fillRect(20, 250, tft.width(), tft.height(), HX8357_BLACK);
        drawCentreString("Stop touching the Screen...", tft.width(), tft.height() - 20);
        //First_Start_Menu();
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        break;
        }
        
      if (touch_count <= 100 & p.z < MINPRESSURE) {
        Flags.p_x_was = Flags.px_average;
        Flags.p_y_was = Flags.py_average;
        Serial.print("p_x_was got set to: ");Serial.println(Flags.p_x_was);
        Serial.print("p_y_was got set to: ");Serial.println(Flags.p_y_was);
        break;
        }
      }   

     if (Flags.p_y_was > 0 && Flags.p_y_was < tft.height()) {
        if (Flags.p_x_was > 0 && Flags.p_x_was < tft.width()) {
          Serial.println("This might be working");
          Flags.Start_Menu = true;
        }
      }
    } 
    Serial.println(Flags.p_x_was); Serial.println(Flags.p_y_was); //delay(3000);
    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0;
    Serial.println(Flags.p_x_was);Serial.println(Flags.p_y_was);
    //delay(5000);
}
/*----------------------------------SUBROUTINE-----------------------------------------------//
//-------------------------------**Set_Run_Mode**--------------------------------------------//
//------------------Sets run mode to indef or timed (with time-out)--------------------------*/

void Set_Run_Mode() {
  Serial.println("Clock mode select menu was called: ");
  //seal.bmp is 210 x 230 pix
  tft.fillRect(32+210, 50, tft.width(), tft.height(), HX8357_BLACK);
  tft.fillRect(0, (tft.height()/2) + 230/2, tft.width(), tft.height(), HX8357_BLACK);
  //reader.drawBMP("/seal_only.bmp", tft, 32, 72);
  tft.setTextColor(HX8357_WHITE);
  Serial.print(tft.width());
  tft.setFont(&FreeSerifBold12pt7b);
  tft.setCursor(310, 95);  // font is 16 pix high 
  tft.println("Start Here:");
  tft.fillRect(308, 98, 119, 2, HX8357_WHITE); // Draws white underline for "Start Here:"
  tft.drawRect(298, 142, 140, 38, HX8357_WHITE);
  tft.setFont(&FreeSerifBold9pt7b);
  tft.setCursor(310, 166);
  tft.println("With Time-out");  // font is 12 pix high
  tft.drawRect(298, 218, 162, 38, HX8357_WHITE); //yep, this is all hard coded because I can't figure out drawCentreString
  tft.setCursor(310, 242);
  tft.println("Without Time-out");

  while (Flags.Run_Mode_Menu != 1) {

  TSPoint p = ts.getPoint();

  Serial.println("In run mode select loop");
  Serial.print("\tPressure= "); Serial.println(p.z);

    int touch_count = 0;
    while (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      TSPoint p = ts.getPoint();
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
      Serial.print("P.z is: ");Serial.println(p.z);
      touch_count ++;
      
      Serial.print("touch_count is: "); Serial.println(touch_count);

      //Running average of p.x and p.y

      Flags.px_sum = Flags.px_sum + p.x;
      Flags.py_sum = Flags.py_sum + p.y;
      Flags.pz_sum = Flags.pz_sum + p.z;
      Flags.px_average = Flags.px_sum / touch_count;
      Flags.py_average = Flags.py_sum / touch_count;
      Flags.pz_average = Flags.pz_sum / touch_count;
      Serial.print("Averaged p.x and p.y values: ");Serial.print(Flags.px_average); Serial.print(" ");Serial.print(Flags.py_average); 
      
      if (touch_count > 100 & p.z > MINPRESSURE) {
        tft.fillRect(20, 260, tft.width(), tft.height(), HX8357_BLACK);
        drawCentreString("Stop touching the Screen...", tft.width(), tft.height() - 20);
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        //Set_Run_Mode();
        //delay(10000);
        break;
        }
        
      if (touch_count <= 100 & p.z < MINPRESSURE) {
        Flags.p_x_was = Flags.px_average;
        Flags.p_y_was = Flags.py_average;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        Serial.print("p_x_was got set to: ");Serial.println(Flags.p_x_was);
        Serial.print("p_y_was got set to: ");Serial.println(Flags.p_y_was);
        break;
        }
      }   

     if (Flags.p_y_was > 23 && Flags.p_y_was < 118) {
        if (Flags.p_x_was > 214 && Flags.p_x_was < 273) {
          Serial.println("Past touch control loop");
          tft.drawRect(298, 142, 140, 38, HX8357_RED);
          Flags.Run_Mode = 'T';
          Flags.Run_Mode_Menu = 1;
          //break;
        }
     }
      if (Flags.p_y_was > 9 && Flags.p_y_was < 118) {
        if (Flags.p_x_was > 332 && Flags.p_x_was < 387) {
          tft.drawRect(298, 218, 162, 38, HX8357_RED);
          Flags.Run_Mode = 'I';
          Flags.Run_Mode_Menu = 1;
          //break;
        } 
      }
      else {
        Flags.p_y_was = 0; 
        Flags.p_x_was = 0;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;}
      Serial.print("Flags are now: "); Serial.print(Flags.p_y_was); Serial.println(Flags.p_x_was);Serial.println(Flags.Run_Mode);
      //delay(3000);
    }
    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0;
    Serial.println("Exiting Run_Mode control loop"); Serial.println(Flags.Run_Mode); //delay(3000);
}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//----------------------------------**Set_Clock**--------------------------------------------//
//-----------------Not used currently, will be needed for clock module-----------------------*/

void Set_Clock() {
  tft.fillRect(0, 50, tft.width(), tft.height(), HX8357_BLACK);
  int start_offset = 80;
  //These for loops draw the Keypad UI for 0-9 and ":" and "-"
  for (int i = 0 ; i  < 3; i++) { // width vector
    for (int j = 0 ; j  < 4; j++) { // height vector
    tft.fillRect(COL_SPACER + i * BOX_WIDTH/1.5, start_offset + ROW_SPACER + j * BOX_HEIGHT/1.5, BOX_WIDTH/2, BOX_HEIGHT/2, HX8357_YELLOW);
    //tft.setCursor(COL_SPACER + 2 * BOX_WIDTH + ((BOX_WIDTH - (strlen(TS_Class_List[i]) * 12)) / 2), i * BOX_HEIGHT + j * ROW_SPACER + 14);
    //tft.println(TS_Class_List[i]);
    }
  }
  tft.setTextColor(HX8357_WHITE);
  //Serial.print(tft.width());
  tft.setFont(&FreeSerifBold12pt7b);
  //tft.setCursor(310, 95);  // font is 16 pix high 
  drawCentreString("Enter as: yyyy-mm-dd hh:mm:ss", tft.width(), 60);
  tft.fillRect(308, 98, 119, 2, HX8357_WHITE); // Draws white underline for "Start Here:"
  tft.drawRect(298, 142, 140, 38, HX8357_WHITE);
  tft.setFont(&FreeSerifBold9pt7b);
  tft.setCursor(310, 166);
  tft.println("With Time-out");  // font is 12 pix high
  tft.drawRect(298, 218, 162, 38, HX8357_WHITE); //yep, this is all hard coded because I can't figure out drawCentreString
  tft.setCursor(310, 242);
  tft.println("Without Time-out");
  
while (Flags.Set_Clock_Menu == false) {

  TSPoint p = ts.getPoint();
  int x, y, x1, y1, w, h;
  static uint8_t secs, mins, hours;
  DateTime now = DS3231M.now();
  if (secs != now.second())// Output if seconds have changed
  {
    // Use sprintf() to pretty print the date/time with leading zeros
    char output_buffer_date[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    char output_buffer_time[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    sprintf(output_buffer_date, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    sprintf(output_buffer_time, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.setCursor(310, 95);  // font is 16 pix high
    tft.println(output_buffer_date);
    tft.setCursor(310, 115);  // font is 16 pix high
    tft.getTextBounds(output_buffer_time, x, y, &x1, &y1, &w, &h);
    tft.fillRect(358, 115-h, 20, h+2, HX8357_BLACK);
    
    //Draw over the minutes when the minutes change
    if (mins != now.minute()){
      tft.fillRect(333, 115-h, 19, h+2, HX8357_BLACK);
    } mins = now.minute();
    
    //Draw over the hours when the hours change
    if (hours != now.hour()){
      tft.fillRect(310, 115-h, 50, h+2, HX8357_BLACK);
    } hours = now.hour();

    tft.println(output_buffer_time);
    secs = now.second();
    //delay(3000);
  }
   
  Serial.println("In set clock loop");
  Serial.print("\tPressure= "); Serial.println(p.z);

    int touch_count = 0;
    while (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      TSPoint p = ts.getPoint();
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
      Serial.print("P.z is: ");Serial.println(p.z);
      touch_count ++;
      
      Serial.print("touch_count is: "); Serial.println(touch_count);

      //Running average of p.x and p.y

      Flags.px_sum = Flags.px_sum + p.x;
      Flags.py_sum = Flags.py_sum + p.y;
      Flags.pz_sum = Flags.pz_sum + p.z;
      Flags.px_average = Flags.px_sum / touch_count;
      Flags.py_average = Flags.py_sum / touch_count;
      Flags.pz_average = Flags.pz_sum / touch_count;
      Serial.print("Averaged p.x and p.y values: ");Serial.print(Flags.px_average); Serial.print(" ");Serial.print(Flags.py_average);
      
      if (touch_count > 100 & p.z > MINPRESSURE) {
        //tft.fillRect(20, 250, tft.width(), tft.height(), HX8357_BLACK);
        //drawCentreString("Stop touching the Screen...", tft.width(), tft.height() - 20);
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        //delay(10000);
        break;
        }
        
      if (touch_count <= 100 & p.z < MINPRESSURE) {
        Flags.p_x_was = Flags.px_average;
        Flags.p_y_was = Flags.py_average;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        Serial.print("p_x_was got set to: ");Serial.println(Flags.p_x_was);
        Serial.print("p_y_was got set to: ");Serial.println(Flags.p_y_was);
        break;
        }
      }   
     //Column (1, 4, 7, -)
     String pad_text = ""; String input_buf; // does pad_text need to be a string?
     if (Flags.p_y_was > 266 && Flags.p_y_was < 304) {
        if (Flags.p_x_was > 129 && Flags.p_x_was < 197) { // Selects "1"
          pad_text = "1";
          input_buf += pad_text;
          Serial.println(pad_text);
          Serial.print("input_buf is: "); Serial.println(input_buf);
          //delay(1000);
        }
        if (Flags.p_x_was > 220 && Flags.p_x_was < 279) { // Selects "4"
          pad_text = "4";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 290 && Flags.p_x_was < 342) { // Selects "7"
          pad_text = "7";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 357 && Flags.p_x_was < 424) { // Selects "-"
          pad_text = "-";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
     }
     //Row 2 (2, 5, 8, 0)
     if (Flags.p_y_was > 211 && Flags.p_y_was < 253) {
        if (Flags.p_x_was > 129 && Flags.p_x_was < 197) { // Selects "2"
          pad_text = "2";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 220 && Flags.p_x_was < 279) { // Selects "5"
          pad_text = "5";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 290 && Flags.p_x_was < 342) { // Selects "8"
          pad_text = "8";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 357 && Flags.p_x_was < 424) { // Selects "0"
          pad_text = "0";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
     }
      //Row 3 (3, 6, 9, :)
     if (Flags.p_y_was > 158 && Flags.p_y_was < 200) {
        if (Flags.p_x_was > 129 && Flags.p_x_was < 197) { // Selects "3"
          pad_text = "3";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 220 && Flags.p_x_was < 279) { // Selects "6"
          pad_text = "6";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 290 && Flags.p_x_was < 342) { // Selects "9"
          pad_text = "9";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
        if (Flags.p_x_was > 357 && Flags.p_x_was < 424) { // Selects ":"
          pad_text = ":";
          input_buf += pad_text;
          Serial.println(pad_text);
        }
     }
     else {
        Flags.p_y_was = 0; 
        Flags.p_x_was = 0;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;}
      Serial.print("Flags are now: "); Serial.print(Flags.p_y_was); Serial.println(Flags.p_x_was);
      //delay(3000);
    tft.setCursor(50, 50);
    tft.setTextColor(HX8357_WHITE);
    tft.drawRect(50, 50, 50, 16, HX8357_WHITE);
    int len = input_buf.length();
    char print_buf;
    input_buf.toCharArray(print_buf, len);
    Serial.print("print buf is: ");Serial.println(print_buf);
    tft.println("print the shit here");
    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0;
    Serial.println("Exiting Clock set control loop");
  }
  
//  Wire.endTransmission();
//  Wire.beginTransmission(addr_DS3231); // communicate with the RTC via I2C
//  Wire.write(0x00); // seconds time register
//  Wire.write(B00111001); // write register bitmap for 39 seconds
//  Wire.endTransmission();
//  Wire.beginTransmission(addr_DS3231); // communicate with the RTC via I2C
//  Wire.write(0x01); // minutes time register
//  Wire.write(B00111001); // write register bitmap for 39 minutes
//  Wire.endTransmission(); 

}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//--------------------------------**Center_Text**--------------------------------------------//
//--------------------Helper function for drawing text "align center"------------------------*/

void drawCentreString(const char *buf, int x, int y)
{
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
  tft.setCursor((x - w) / 2, y);
  tft.print(buf);
//  Serial.println("Current font height is: "); Serial.print(h);
//  delay(3000);
}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//------------------------------**Class_Select_Page**----------------------------------------//
//--------------Draw a bunch of security levels, register the chosen one---------------------*/

void Class_Select_Page() {

  //Create columns for security selection:
  //Internal 
  tft.fillScreen(HX8357_BLACK);
  tft.setFont();
  tft.fillRect(COL_SPACER, ROW_SPACER, BOX_WIDTH, BOX_HEIGHT, HX8357_GREEN);
  tft.setCursor(COL_SPACER+(25), ROW_SPACER+10); // ***ALL HARD CODED FOR NOW***FIX LATER***
  tft.setTextColor(HX8357_WHITE);  tft.setTextSize(2);
  tft.println("Internal Only");

  //Company Confidental column
  const int C_NUM_ELEMENTS = 3;
  const int C_MAX_SIZE = 7;
  char Conf_Class_List [C_NUM_ELEMENTS] [C_MAX_SIZE] = { // This is a character array. There are no strings here?
    { "Confidental" },
    { "CONF Lvl 1" },
    { "CONF Lvl 2" },
  };
  for (int i = 0, j = 1; i < C_NUM_ELEMENTS, j < C_NUM_ELEMENTS + 1; i++, j++) {
    tft.fillRect(2 * COL_SPACER + BOX_WIDTH, j * ROW_SPACER + i * BOX_HEIGHT, BOX_WIDTH, BOX_HEIGHT, HX8357_RED);
    tft.setCursor(2 * COL_SPACER + BOX_WIDTH + ((BOX_WIDTH - (strlen(Conf_Class_List[i]) * 12)) / 2), i * BOX_HEIGHT + j * ROW_SPACER + 30);
    tft.println(Conf_Class_List[i]);
  }

  //Restricted column
  tft.setTextColor(HX8357_BLACK);
  const int R_NUM_ELEMENTS = 3;
  const int R_MAX_SIZE = 10;
  char R_Class_List [R_NUM_ELEMENTS] [R_MAX_SIZE] = {
    { "Restricted" },
    { "RES Lvl 1" },
    { "RES Lvl 2" },
  };
  for (int i = 0, j = 1; i < R_NUM_ELEMENTS, j < R_NUM_ELEMENTS + 1; i++, j++) {
    tft.fillRect(3 * COL_SPACER + 2 * BOX_WIDTH, j * ROW_SPACER + i * BOX_HEIGHT, BOX_WIDTH, BOX_HEIGHT, HX8357_YELLOW);
    tft.setCursor(3 * COL_SPACER + 2 * BOX_WIDTH + ((BOX_WIDTH - (strlen(R_Class_List[i]) * 12)) / 2), i * BOX_HEIGHT + j * ROW_SPACER + 30);
    tft.println(R_Class_List[i]);
  }

  while (Flags.Class_Menu != 1){

  TSPoint p = ts.getPoint();

  Serial.println("In run mode select loop");
  Serial.print("\tPressure= "); Serial.println(p.z);

    int touch_count = 0;
    while (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      TSPoint p = ts.getPoint();
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
      Serial.print("P.z is: ");Serial.println(p.z);
      touch_count ++;
      
      Serial.print("touch_count is: "); Serial.println(touch_count);

      //Running average of p.x and p.y

      Flags.px_sum = Flags.px_sum + p.x;
      Flags.py_sum = Flags.py_sum + p.y;
      Flags.pz_sum = Flags.pz_sum + p.z;
      Flags.px_average = Flags.px_sum / touch_count;
      Flags.py_average = Flags.py_sum / touch_count;
      Flags.pz_average = Flags.pz_sum / touch_count;
      Serial.print("Averaged p.x and p.y values: ");Serial.print(Flags.px_average); Serial.print(" ");Serial.print(Flags.py_average); 
      
      if (touch_count > 100 & p.z > MINPRESSURE) {
        //tft.fillRect(20, 250, tft.width(), tft.height(), HX8357_BLACK);
        //drawCentreString("Stop touching the Screen...", tft.width(), tft.height() - 20);
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        //delay(7000);
        break;
        }
        
      if (touch_count <= 100 & p.z < MINPRESSURE) {
        Flags.p_x_was = Flags.px_average;
        Flags.p_y_was = Flags.py_average;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;;
        Serial.print("p_x_was got set to: ");Serial.println(Flags.p_x_was);
        Serial.print("p_y_was got set to: ");Serial.println(Flags.p_y_was);
        break;
        }
      }   

     if (Flags.p_y_was > 228 && Flags.p_y_was < 308) { // Col 1, UNCLASS
        if (Flags.p_x_was > 20 && Flags.p_x_was < 136) {
          Serial.println("This might be working");
          Brief_Class.str_class  = "Internal";
          Brief_Class.Red = 0; Brief_Class.Green = 7; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        }
     }
      if (Flags.p_y_was > 131 && Flags.p_y_was < 215) { // Col 2, SECRET
        if (Flags.p_x_was > 20 && Flags.p_x_was < 136) {
          Brief_Class.str_class = "CONF";
          Brief_Class.Red = 7; Brief_Class.Green = 0; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        }
                if (Flags.p_x_was > 156 && Flags.p_x_was < 277) {
          Brief_Class.str_class = "CONF Lvl 1";
          Brief_Class.Red = 7; Brief_Class.Green = 0; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        }
                if (Flags.p_x_was > 294 && Flags.p_x_was < 415) {
          Brief_Class.str_class = "CONF Lvl 2";
          Brief_Class.Red = 7; Brief_Class.Green = 0; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        } 
      }
        if (Flags.p_y_was > 36 && Flags.p_y_was < 119) { // Col 3, TS
          if (Flags.p_x_was > 20 && Flags.p_x_was < 136) {
          Brief_Class.str_class = "Restricted";
          Brief_Class.Red = 7; Brief_Class.Green = 2; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        }
                if (Flags.p_x_was > 156 && Flags.p_x_was < 277) {
          Brief_Class.str_class = "RES Lvl 1";
          Brief_Class.Red = 7; Brief_Class.Green = 7; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        }
                if (Flags.p_x_was > 294 && Flags.p_x_was < 415) {
          Brief_Class.str_class = "RES Lvl 2";
          Brief_Class.Red = 7; Brief_Class.Green = 7; Brief_Class.Blue = 0;
          Flags.Class_Menu = 1;
          break;
        } 
      }
      else {
        Flags.p_y_was = 0; 
        Flags.p_x_was = 0;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;}
      Serial.println("do we ever get here");
      Serial.print("Flags are now: "); Serial.print(Flags.p_y_was); Serial.println(Flags.p_x_was);
      //delay(3000);
    }
    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0; 
}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//-------------------------------**Begin_Execution**-----------------------------------------//
//--------------------Confirm all settings, call Set LED subroutine--------------------------*/

void Begin_Execution() {
  tft.setTextSize(1);
  tft.fillScreen(HX8357_BLACK);
  tft.setTextColor(HX8357_WHITE);
  tft.setFont(&FreeSerifBold18pt7b);
  drawCentreString("Confirm?", tft.width(), 30); // font is 31 pix high
  tft.setFont(&FreeSerifBold12pt7b);
  //tft.fillRect((tft.width() - BOX_WIDTH) / 2, (tft.height() - BOX_HEIGHT) / 2, BOX_WIDTH, BOX_HEIGHT, HX8357_RED);
  drawCentreString("Classification: ", tft.width(), tft.height()/2);
  tft.setCursor((tft.width()/2)-(45),tft.height()/2 + 25);
  tft.println(Brief_Class.str_class);
  tft.fillRect(298, 218, 162, 38, HX8357_RED); //yep, this is all hard coded because I can't figure out drawCentreString
  tft.setCursor(315, 242);
  tft.println("Cancel");
  tft.fillRect(30, 218, 162, 38, HX8357_RED); //yep, this is all hard coded because I can't figure out drawCentreString
  tft.setCursor(42, 242);
  tft.println("Confirm");
  
  while (Flags.Begin_LED != 1) {
    
      TSPoint p = ts.getPoint();

      int touch_count = 0;
    while (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      TSPoint p = ts.getPoint();
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
      touch_count ++;

      //Running average of p.x and p.y

      Flags.px_sum = Flags.px_sum + p.x;
      Flags.py_sum = Flags.py_sum + p.y;
      Flags.pz_sum = Flags.pz_sum + p.z;
      Flags.px_average = Flags.px_sum / touch_count;
      Flags.py_average = Flags.py_sum / touch_count;
      Flags.pz_average = Flags.pz_sum / touch_count;
      Serial.print("Averaged p.x and p.y values: ");Serial.print(Flags.px_average); Serial.print(" ");Serial.println(Flags.py_average);
      if (touch_count > 100 & p.z > MINPRESSURE) {
        //tft.fillRect(20, 250, tft.width(), tft.height(), HX8357_BLACK);
        //drawCentreString("Stop touching the Screen...", tft.width(), tft.height() - 20);
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;
        //delay(7000);
        break;
        }
        
      if (touch_count <= 100 & p.z < MINPRESSURE) {
        Flags.p_x_was = Flags.px_average;
        Flags.p_y_was = Flags.py_average;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;
        touch_count = 0;;
        //Serial.print("p_x_was got set to: ");Serial.println(Flags.p_x_was);
        //Serial.print("p_y_was got set to: ");Serial.println(Flags.p_y_was);
        break;
        }
    }   

     if (Flags.p_y_was > 9 && Flags.p_y_was < 118) {
        if (Flags.p_x_was > 332 && Flags.p_x_was < 387) {
          Cancel();
          break;
        }
     }

     if (Flags.p_y_was > 191 && Flags.p_y_was < 301) {
        if (Flags.p_x_was > 330 && Flags.p_x_was < 387) {
          Flags.Ready_to_run = 1;
          Flags.Begin_LED = 1;
        }
     }
      else {
        Flags.p_y_was = 0; 
        Flags.p_x_was = 0;
        Flags.px_sum = 0;
        Flags.py_sum = 0;
        Flags.px_average = 0;
        Flags.py_average = 0;}
      Serial.println("do we ever get here");
      Serial.print("Flags are now: "); Serial.print(Flags.p_y_was); Serial.println(Flags.p_x_was);
    }
    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0; 
}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//-----------------------------------**Cancel**----------------------------------------------//
//----------------Cancels display. Calls setup() again to clear settings---------------------*/

void Cancel() {

  tft.fillScreen(HX8357_BLACK);
  //tft.fillRect((tft.width() - BOX_WIDTH * 2) / 2, (tft.height() - BOX_HEIGHT * 2) / 2, BOX_WIDTH * 2, BOX_HEIGHT * 2, HX8357_RED);
  tft.setCursor((tft.width() - BOX_WIDTH * 2) / 2, (tft.height() - BOX_HEIGHT * 2) / 2);
  tft.setTextColor(HX8357_WHITE);
  tft.setTextSize(1);
  tft.setFont(&FreeSerifBold18pt7b);
  drawCentreString("HOLD 5 SEC TO CANCEL", tft.width(), 175);

    Flags.px_sum = 0;
    Flags.py_sum = 0;
    Flags.px_average = 0;
    Flags.py_average = 0;
    Flags.p_x_was = 0;
    Flags.p_y_was = 0;

  while (Flags.Cancel_Disp != 1) {
  
  Serial.println("Cancel was called");
    // Get user input for the classification
    // Retrieve a point
  TSPoint p = ts.getPoint();

  tft.setTextSize(1);
  tft.setFont(&FreeSerifBold9pt7b);
  
  int x, y, x1, y1, w, h;
  static uint8_t secs, mins, hours;
  DateTime now = DS3231M.now();
  if (secs != now.second())// Output if seconds have changed
  {
    // Use sprintf() to pretty print the date/time with leading zeros
    char output_buffer_date[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    char output_buffer_time[SPRINTF_BUFFER_SIZE];  ///< Temporary buffer for sprintf()
    sprintf(output_buffer_date, "%04d-%02d-%02d", now.year(), now.month(), now.day());
    sprintf(output_buffer_time, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.setCursor(310, 95);  // font is 16 pix high
    tft.println(output_buffer_date);
    tft.setCursor(310, 115);  // font is 16 pix high
    tft.getTextBounds(output_buffer_time, x, y, &x1, &y1, &w, &h);
    tft.fillRect(358, 115-h, 20, h+2, HX8357_BLACK);
    
    //Draw over the minutes when the minutes change
    if (mins != now.minute()){
      tft.fillRect(333, 115-h, 19, h+2, HX8357_BLACK);
    } mins = now.minute();
    
    //Draw over the hours when the hours change
    if (hours != now.hour()){
      tft.fillRect(310, 115-h, 50, h+2, HX8357_BLACK);
    } hours = now.hour();

    tft.println(output_buffer_time);
    secs = now.second();
    //delay(3000);
  }

    // we have some minimum pressure we consider 'valid'
    // pressure of 0 means no pressing!

    Serial.print("X = "); Serial.print(p.x);
    Serial.print("\tY = "); Serial.print(p.y);
    Serial.print("\tPressure = "); Serial.println(p.z);


    // Scale from ~0->1000 to tft.width using the calibration #'s
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

    if (p.z > MINPRESSURE && p.z < MAXPRESSURE); {
      if (p.y > 0 && p.y < tft.height()) {
        if (p.x > 0 && p.x < tft.width()) {
          ++Flags.hold_count;
          if (Flags.hold_count > 500) {
            Serial.print(Flags.hold_count);
            matrix.fillScreen(matrix.Color333(0, 0, 0));
            do_nothing_but_reset();
            break;
          }
        }
      }
    }
  }
}

/*-----------------------------------SUBROUTINE----------------------------------------------//
//--------------------------------**Setup for RTC**------------------------------------------//
//--------------------Initialize the RTC. This might not be needed---------------------------*/


//void setupRtc()
//{
///*      get the clock into a known state
//        turn off any Alarms that may be left over from a previous use
//        disable interrupts from the clock
//        make sure the clock is running
// 
//        0 0 0 0 0 0 0 0 
//        | | | | | | | |_____ (A1IE)  - disable Alarm 1 interrupt -- Also LSB
//        | | | | | | |_______ (A2IE)  - disable Alarm 2 interrupt
//        | | | | | |_________ (INTCN) - disable interrupts from alarms
//        | | | | |___________ (RS1)   - 1 Hz square wave (if enabled)
//        | | | |_____________ (RS2)   - 
//        | | |_______________ (CONV)  - do not read chip temperature
//        | |_________________ (BBSQW) - disable square wave output
//        |___________________ (EOSC)  - enable oscillator (low turns clock on) -- Also MSB
//*/
//
//  Wire.beginTransmission(addr_DS3231); // communicate with the RTC via I2C
//  Wire.write(0x0E); // Control register
//  Wire.write(B00000000); // write register bitmap
//}
//
///*-----------------------------------SUBROUTINE----------------------------------------------//
////--------------------------------**getRtcData()**-------------------------------------------//
////-------------------------Read out the time from the RTC------------------------------------*/
//
//void getRtcData()
//{
//  // request seven bytes of data from DS3231 starting with register 00h
//  Wire.beginTransmission(addr_DS3231);
//  Wire.write(0);
//  Wire.endTransmission();
//  Wire.requestFrom(addr_DS3231, 7);  
//
//  // strip out extraneous bits used by the RTC chip and convert BCD to Decimal
//  timeinfo.tm_sec  = bcdToDec(Wire.read() & 0x7F);
//  timeinfo.tm_min  = bcdToDec(Wire.read() & 0x7F);
//  timeinfo.tm_hour = bcdToDec(Wire.read() & 0x3F);
//  timeinfo.tm_wday = bcdToDec(Wire.read() & 0x03);
//  timeinfo.tm_mday = bcdToDec(Wire.read() & 0x3F);
//  timeinfo.tm_mon  = bcdToDec(Wire.read() & 0x1F);
//  timeinfo.tm_year = bcdToDec(Wire.read());
//  
//  // adjust month and year values to agree with time.h format
//  timeinfo.tm_mon = timeinfo.tm_mon - 1;    
//  timeinfo.tm_year = timeinfo.tm_year + 100;  
//}
//
//// .......................................................................
//uint8_t bcdToDec(uint8_t val)
//// Convert binary coded decimal to normal decimal numbers
//{
//  return ( (val/16*10) + (val%16) );
//}
//
//// .......................................................................
//void displayRtcData()
//{
//  // display the date and time in this form --> 2020-03-21 12:34:56
//  struct tm *timeinfoPtr = &timeinfo;       // pointer to timeinfo structure
//  char strfBuf[30];                         // buffer used by strftime()
//  
//  Serial.print("\nThe present date and time is:  ");  
//  strftime(strfBuf, sizeof(strfBuf), "%F %T", timeinfoPtr);  
//  Serial.print(strfBuf);     
//}
