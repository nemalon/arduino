/*
Version 1.0 supports OLED display's with either SDD1306 or with SH1106 controller
*/

#include <Wire.h>
#include "font.h"
#include "Button.h"
#include "DCMotor.h"

#define offset 0x00    // SDD1306                    // offset=0 for SSD1306 controller
//#define offset 0x02    // SH1106                       // offset=2 for SH1106 controller
#define OLED_address  0x3c                             // all the OLED's I have seen have this address

#define PUMP_SLICE 100 /* seconds */

/* OLED/LCD */ 
#define SCL   D2
#define SDA   D1

int  pump_time = 0;

Button pushButton_inc (D5);
Button pushButton_dec (D8);
Button pushButton_stop(D7);
DCMotor        dcMotor(D4);

void printInfo(bool bInc, bool bDec, bool bStop)
{
  char buffer[16];

  long current = millis();
  long uptime_sec = current/1000; //convect milliseconds to seconds
  
  if ( dcMotor.state() ) 
    sendStrXY("Water Pump is on", 0 ,0);
  else
    sendStrXY("Water Pump ready", 0 ,0);

  // Jonathan 
  sendStrXY("by Jonathan", 1, 0);
  
  // input 
  //memset(buffer,0, 16);
  //sprintf(buffer, "inc:%d dec:%d on:%d", bInc, bDec, bStop);
  //sendStrXY(buffer, 2 ,0);

  // time left 
  memset(buffer,0, 16);
  sprintf(buffer, "ml: %08d", pump_time);
  sendStrXY(buffer, 3 ,0);

  // uptime
  memset(buffer,0, 16);
  sprintf(buffer, "uptime: %lu", uptime_sec);
  sendStrXY(buffer, 5 ,0);  
}

void pumpControl(int bInc, int bDec, int bStop)
{
    if (bInc)
      pump_time += PUMP_SLICE;
    if (bDec)
      pump_time -= PUMP_SLICE;
      
    // do we need to change motor state
    if (pump_time && bStop) 
    {
        if ( !dcMotor.state() )
          dcMotor.on(200);
        else
          dcMotor.off();
    }

    if ( dcMotor.state() )
      --pump_time;

    if (pump_time <= 0) {
      pump_time = 0;
      dcMotor.off(); 
    }
}

void setup(void) {
//ESP.wdtDisable();                               // used to debug, disable wachdog timer, 
  Serial.begin(115200);                           // full speed to monitor
  Wire.begin(SDA,SCL);                            // Initialize I2C and OLED Display
  init_OLED();                                    // 
  reset_display();

  pushButton_inc.init();
  pushButton_dec.init();
  pushButton_stop.init();
  dcMotor.off();
    
  clear_display();
  sendStrXY("Water Pump ready" , 0, 0);
}


void loop(void) {
  bool bInc  = pushButton_inc.read();
  bool bDec  = pushButton_dec.read();
  bool bStop = pushButton_stop.read();
  pumpControl(bInc, bDec, bStop);
  printInfo(bInc, bDec, bStop);
}

//==========================================================//
// Resets display depending on the actual mode.
static void reset_display(void)
{
  displayOff();
  clear_display();
  displayOn();
}

//==========================================================//
// Turns display on.
void displayOn(void)
{
  sendcommand(0xaf);        //display on
}

//==========================================================//
// Turns display off.
void displayOff(void)
{
  sendcommand(0xae);    //display off
}

//==========================================================//
// Clears the display by sendind 0 to all the screen map.
static void clear_display(void)
{
  unsigned char i,k;
  for(k=0;k<8;k++)
  { 
    setXY(k,0);    
    {
      for(i=0;i<(128 + 2 * offset);i++)     //locate all COL
      {
        SendChar(0);         //clear all COL
        //delay(10);
      }
    }
  }
}

//==========================================================//
// Actually this sends a byte, not a char to draw in the display. 
// Display's chars uses 8 byte font the small ones and 96 bytes
// for the big number font.
static void SendChar(unsigned char data) 
{
  //if (interrupt && !doing_menu) return;   // Stop printing only if interrupt is call but not in button functions
  
  Wire.beginTransmission(OLED_address); // begin transmitting
  Wire.write(0x40);//data mode
  Wire.write(data);
  Wire.endTransmission();    // stop transmitting
}

//==========================================================//
// Prints a display char (not just a byte) in coordinates X Y,
// being multiples of 8. This means we have 16 COLS (0-15) 
// and 8 ROWS (0-7).
static void sendCharXY(unsigned char data, int X, int Y)
{
  setXY(X, Y);
  Wire.beginTransmission(OLED_address); // begin transmitting
  Wire.write(0x40);//data mode
  
  for(int i=0;i<8;i++)          
    Wire.write(pgm_read_byte(myFont[data-0x20]+i));
    
  Wire.endTransmission();    // stop transmitting
}

//==========================================================//
// Used to send commands to the display.
static void sendcommand(unsigned char com)
{
  Wire.beginTransmission(OLED_address);     //begin transmitting
  Wire.write(0x80);                          //command mode
  Wire.write(com);
  Wire.endTransmission();                    // stop transmitting
}

//==========================================================//
// Set the cursor position in a 16 COL * 8 ROW map.
static void setXY(unsigned char row,unsigned char col)
{
  sendcommand(0xb0+row);                //set page address
  sendcommand(offset+(8*col&0x0f));       //set low col address
  sendcommand(0x10+((8*col>>4)&0x0f));  //set high col address
}


//==========================================================//
// Prints a string regardless the cursor position.
static void sendStr(unsigned char *string)
{
  unsigned char i=0;
  while(*string)
  {
    for(i=0;i<8;i++)
    {
      SendChar(pgm_read_byte(myFont[*string-0x20]+i));
    }
    *string++;
  }
}

//==========================================================//
// Prints a string in coordinates X Y, being multiples of 8.
// This means we have 16 COLS (0-15) and 8 ROWS (0-7).
static void sendStrXY( const char *string, int X, int Y)
{
  setXY(X,Y);
  unsigned char i=0;
  while(*string)
  {
    for(i=0;i<8;i++)
    {
      SendChar(pgm_read_byte(myFont[*string-0x20]+i));
    }
    *string++;
  }
}


//==========================================================//
// Inits oled and draws logo at startup
static void init_OLED(void)
{
  sendcommand(0xae);    //display off
  sendcommand(0xa6);            //Set Normal Display (default)
    // Adafruit Init sequence for 128x64 OLED module
    sendcommand(0xAE);             //DISPLAYOFF
    sendcommand(0xD5);            //SETDISPLAYCLOCKDIV
    sendcommand(0x80);            // the suggested ratio 0x80
    sendcommand(0xA8);            //SSD1306_SETMULTIPLEX
    sendcommand(0x3F);
    sendcommand(0xD3);            //SETDISPLAYOFFSET
    sendcommand(0x0);             //no offset
    sendcommand(0x40 | 0x0);      //SETSTARTLINE
    sendcommand(0x8D);            //CHARGEPUMP
    sendcommand(0x14);
    sendcommand(0x20);             //MEMORYMODE
    sendcommand(0x00);             //0x0 act like ks0108
    
    //sendcommand(0xA0 | 0x1);      //SEGREMAP   //Rotate screen 180 deg
    sendcommand(0xA0);
    
    //sendcommand(0xC8);            //COMSCANDEC  Rotate screen 180 Deg
    sendcommand(0xC0);
    
    sendcommand(0xDA);            //0xDA
    sendcommand(0x12);           //COMSCANDEC
    sendcommand(0x81);           //SETCONTRAS
    sendcommand(0xCF);           //
    sendcommand(0xd9);          //SETPRECHARGE 
    sendcommand(0xF1); 
    sendcommand(0xDB);        //SETVCOMDETECT                
    sendcommand(0x40);
    sendcommand(0xA4);        //DISPLAYALLON_RESUME        
    sendcommand(0xA6);        //NORMALDISPLAY             

  clear_display();
  sendcommand(0x2e);            // stop scroll
  //----------------------------REVERSE comments----------------------------//
    sendcommand(0xa0);    //seg re-map 0->127(default)
    sendcommand(0xa1);    //seg re-map 127->0
    sendcommand(0xc8);
    delay(1000);
  //----------------------------REVERSE comments----------------------------//
  // sendcommand(0xa7);  //Set Inverse Display  
  // sendcommand(0xae);   //display off
  sendcommand(0x20);            //Set Memory Addressing Mode
  sendcommand(0x00);            //Set Memory Addressing Mode ab Horizontal addressing mode
  //  sendcommand(0x02);         // Set Memory Addressing Mode ab Page addressing mode(RESET)  
}



