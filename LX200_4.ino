/// ESP32 + OLED PRINTING LIKE Serial.print but in OLED ///
/// also support printing on OLED for BLYNK_PRINT messages ///
//#define BLYNK_DEBUG // Optional, this enables lots of prints
//#define BLYNK_PRINT Serial

#define I2C_ADDRESS 0x3C /// default for HELTEC OLED
#define sda_pin 4
#define scl_pin 15
#define wire_Frequency 200000   /// could be in the range of 100000 to 400000
#define rst_pin 16  /// <== delete this line if your OLED does not have/support OLED reset pin 

//#define BLYNK_DEBUG
#define BLYNK_MAX_READBYTES 1024
#define BLYNK_MSG_LIMIT 0
#define BLYNK_HEARTBEAT 60
#define BLYNK_NO_BUILTIN  // Disable Blynk's built-in analog & digital pin operations

#define HARDWARE_LED 25 /// HELTEC LED

/// Example scrolling display for 64 pixel high display that is 128x64 ///

/// #include <Wire.h> /// not necessary as it is included by command #include "SSD1306AsciiWire.h" bellow
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

SSD1306AsciiWire oled;

int chPos = 0;
// Define a custom console for OLED debug output
class CustomConsole
  : public Print // This automatically adds functions like .println()
{
  public:
    CustomConsole() {}
    virtual size_t write(uint8_t ch) {  // Grabs character from print
       if (chPos >= 20 || char(ch)=='\n') { /// auto line splitting if line is bigger than 20 characters, font depended ...
          chPos = 0;
//          oled.print('\n');
          Serial.print(char(ch));  // Repeat all character output to Serial monitor

      }
      if ( char(ch)!='\n') {
//          oled.print(char(ch));
          Serial.print(char(ch));  // Repeat all character output to Serial monitor
          chPos++ ;
      }
      return 1; // Processed 1 character
    }
  private:
    // define variables used in myConsole
};

// initiate myConsole
CustomConsole myConsole;
#define BLYNK_PRINT myConsole

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESPmDNS.h>  // For OTA
#include <WiFiUdp.h>  // For OTA
#include <ArduinoOTA.h>  // For OTA
#include <TimeLib.h>
#include <WidgetRTC.h>

char auth[] = "xxxxxxxxxxxxxxxxxxxxxxx"; /// put your Blynk auth here ///
char ssid[] = "xxxxxx"; /// put your WiFi name ///
char pass[] = "xxxxxxxxxxxx"; /// put your WiFi password ///

///char server[] = "10.10.3.13"; /// in case of local server etc ... ///
///int port = 8080;

char currentTime[9];
char currentDate[11];

BlynkTimer timer;
WidgetRTC rtc;
WidgetLCD lcd(V20);
//HardwareSerial Serial2(2);

void setup() {
  Serial.begin(115200);
  /// IT IS VERY IMPORTAND TO GIVE YOUR VALUES IN THE Wire.begin(...) bellow Wire.begin(sda_pin, scl_pin, wire_Frequency); form 100000 to 400000
  Wire.begin(sda_pin,scl_pin, wire_Frequency); /// wire_Frequency: form 100000 to 400000
  #ifdef rst_pin
  pinMode(rst_pin,OUTPUT); /// THIS IS THE RESET PIN OF HELTEC OLED ... ///
  digitalWrite(rst_pin, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(rst_pin, HIGH); // while OLED is running, must set GPIO16 in high  
  #endif
  oled.begin(&Adafruit128x64, I2C_ADDRESS); /// in this demo sketch the selected resolution is 128x64
  oled.setFont(System5x7); /// you can change this font but always choose monospaced font for "defined" characters length per line...
  ///    oled.setFont(Adafruit5x7);

  #if INCLUDE_SCROLLING == 0
  #error INCLUDE_SCROLLING must be non-zero. Edit SSD1306Ascii.h
  #elif INCLUDE_SCROLLING == 1
  // Scrolling is not enable by default for INCLUDE_SCROLLING set to one.
  oled.setScroll(true); /// activating the vertical scrolling ///
  #else  // INCLUDE_SCROLLING
  // Scrolling is enable by default for INCLUDE_SCROLLING greater than one.
  #endif
  pinMode(HARDWARE_LED, OUTPUT);  // initialize onboard LED as output  
  digitalWrite(HARDWARE_LED, HIGH); // dim the LED 
      
  timer.setInterval(5010L, TimeDatecheck);
  timer.setInterval(6030L, RSSIcheck);
  timer.setInterval(2045L, FlashLED);
  Serial2.begin(9600, SERIAL_8N1, 18, 17, true);
  Serial2.setTimeout(100);
  WiFi.begin(ssid, pass); /// provided you have put the correct one above ///
  Blynk.config(auth); /// provided you have put the correct one above ///
  Blynk.connect();
  ArduinoOTA.setHostname("HALTEC_ESP32_OLED");  /// For OTA recognition put yours here ///
  ArduinoOTA.begin();  // For OTA
  lcd.clear(); //Use it to clear the LCD Widget
  lcd.print(4, 0, "Optimal Consulting"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
  lcd.print(4, 1, "Meade Classic");
  // Initalisation string for LX200 Classic Base
  Serial2.print(char(0xFF));
  Serial2.print(char(0xFF));
  Serial2.print(char(0xFF));
  Serial2.print(char(0xFF));
  Serial2.print(char(0xFF));
  Serial2.print(char(0x2A));

  
  // Please use timed events when LCD printintg in void loop to avoid sending too many commands
  // It will cause a FLOOD Error, and connection will be dropped
}

BLYNK_CONNECTED() {
  rtc.begin();
  setSyncInterval(360);  // Time Sync
}

void READ_SERIAL2(void) {
    if(Serial2.available()) {
      String ch2 = Serial2.readString();
      int chl = ch2.length();
      int LINETERM = ch2.lastIndexOf(char(0x1B));
      String DISPLAYLINE[4];
      String DISPLAYPREVLINE[4];
      String DISPLAYTEMP;
      int LCDBEGIN;
      int LINESTART = 0;
      int LINEEND = 1;
      int DISPCLEAR = 1;
      myConsole.print("LINE TERM");
      myConsole.println(LINETERM);
      // Check to see if this is display info or if its input data (Eg number input after M)
      if (LINETERM == -1){
        myConsole.println("LineTerm = -1");
        DISPLAYLINE[2] = ch2;
        LCDBEGIN = LCDBEGIN + 2;
        DISPCLEAR = 0;
      } else {
        LCDBEGIN=0;
        // check to see if the end of the line is the same as the end of the input and then use the end of the line
        while (LINEEND != chl){
          // check to see if there is a 1B in the string (indicates start of a command / display)
          LINEEND = ch2.indexOf(char(0x1B),LINESTART+1);
          //myConsole.println(LINEEND);
          // check to see if there is a 00 in the string (00 in the string causes the 1B search to fail and issues with parsing, but this fixes the issue)
          if (LINEEND == -1){
            LINEEND = ch2.indexOf(char(0x00),LINESTART+1);
            myConsole.println("Found a 0x00");
          }
          // make the last line end with the last char
          if (LINEEND == -1){
            LINEEND = chl;
          }
          myConsole.println(LINEEND);
          // find which line number it should be or if -1 ignore (this is for display on the lcd / oled)
          int LINEDISPLAYNO = ch2.substring(LINESTART+1,LINESTART+2).toInt();
          // check to see if its a special function Eg M, or Star etc
          if (ch2.substring(LINESTART+1,LINESTART+2) == "C"){
            LINEDISPLAYNO = 1;
            }
          if (ch2.substring(LINESTART+1,LINESTART+2) == "B"){
            //Clear the screen
            lcd.clear();
            oled.clear();
          }
          myConsole.println(LINEDISPLAYNO);
          myConsole.println(ch2.substring(LINESTART,LINESTART+2));
          if ((LINEDISPLAYNO == 1) or (LINEDISPLAYNO == 2)){
            // take off the start + line number chars
            DISPLAYTEMP = ch2.substring(LINESTART+2,LINEEND);
            myConsole.println(DISPLAYTEMP);
            if (DISPLAYTEMP.length() <= 1){
             // this is a marker char, so add it to the begining of the existing string
             DISPLAYLINE[LINEDISPLAYNO] = DISPLAYTEMP + DISPLAYLINE[LINEDISPLAYNO].substring(1);
             // replace the Tick symbol
             DISPLAYLINE[LINEDISPLAYNO].replace(String(char(0x02)),"*");
             // replace the Degree's symbol
             DISPLAYLINE[LINEDISPLAYNO].replace(String(char(0xDF)),"*");    
            } else {
              DISPLAYLINE[LINEDISPLAYNO] = DISPLAYTEMP;
              DISPLAYLINE[LINEDISPLAYNO].replace(String(char(0x02)),"*");
              DISPLAYLINE[LINEDISPLAYNO].replace(String(char(0xDF)),"*");
            }  
          } else {
            if (ch2.substring(LINESTART+1,LINESTART+2) == "D"){
                // ALT LED
                WidgetLED LED25(V25);
                if (ch2.indexOf(char(0x01),LINESTART+1) != -1){
                  LED25.on();
                } else {
                  LED25.off();                           
                }
            }
            if (ch2.substring(LINESTART+1,LINESTART+2) == "L"){
                WidgetLED LED21(V21);
                WidgetLED LED22(V22);
                WidgetLED LED23(V23);
                WidgetLED LED24(V24);

              if (ch2.indexOf(char(0x08),LINESTART+1) != -1){
                myConsole.println("LED 8");
                LED21.on();
                LED22.off();
                LED23.off();
                LED24.off();
              } else if (ch2.indexOf(char(0x04),LINESTART+1) != -1){
                myConsole.println("LED 4");
                LED21.off();
                LED22.on();
                LED23.off();
                LED24.off();                
              } else if (ch2.indexOf(char(0x02),LINESTART+1) != -1){
                LED21.off();
                LED22.off();
                LED23.on();
                LED24.off();  
              } else if (ch2.indexOf(char(0x02),LINESTART+1) != -1){
                LED21.off();
                LED22.off();
                LED23.off();
                LED24.on();  
              }
              
            }
            myConsole.println("No Line number match");
            myConsole.println(ch2.substring(LINESTART,LINESTART+2));
            myConsole.println(DISPLAYTEMP);
          }
          
          // find which line number it should be
          //int LINEDISPLAYNO = LINEDISPLAY.substring(1,2).toInt(); 
          LINESTART=LINEEND;
          //delay(1);        
        }
      }



      myConsole.println(chl);
      myConsole.println(ch2);
      myConsole.println(DISPLAYLINE[1]);
      myConsole.println(DISPLAYLINE[2]);

      // display the lines correctly
      if (DISPCLEAR == 1){
        if ((DISPLAYLINE[1] != DISPLAYPREVLINE[1]) || (DISPLAYLINE[2] != DISPLAYPREVLINE[2]) ){
             oled.clear();
             oled.print(DISPLAYLINE[1]);
             oled.print('\n');
             oled.print(DISPLAYLINE[2]);
             oled.print('\n');
             lcd.clear();
             lcd.print(0,0,DISPLAYLINE[1]);
             lcd.print(0,1,DISPLAYLINE[2]);
             DISPLAYPREVLINE[1] = DISPLAYLINE[1];
             DISPLAYPREVLINE[2] = DISPLAYLINE[2];
        }
      } else {
        oled.print(DISPLAYLINE[2]);
        lcd.print(LCDBEGIN,1,DISPLAYLINE[2]);
      }
     
      
//      lcd.print(0,0,chl);
//      lcd.print(0,1,ch2);
  }
}

void loop() {
  ArduinoOTA.handle();  // For OTA
  timer.run();
  Blynk.run();
  READ_SERIAL2();
}

void TimeDatecheck() {
  myConsole.printf("%02d:%02d:%02d %02d/%02d/%04d\n",hour(), minute(), second(), month(), day(), year());
}

void RSSIcheck() {
  Blynk.virtualWrite(V30, WiFi.RSSI()); // RSSI
  //myConsole.print("Brd #1 - RSSI:");
  //myConsole.println(WiFi.RSSI());
}

void FlashLED() {
  digitalWrite(HARDWARE_LED, LOW);  // Turn ON
  delay(100);
  digitalWrite(HARDWARE_LED, HIGH);  // Turn OFF
}




BLYNK_WRITE(0) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("ENTER is Down");
      Serial2.print(char(0x0d));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("ENTER is UP");
      Serial2.print(char(0x8d));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(1) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("Mode is Down");
      Serial2.print(char(0x4d));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("Mode is UP");
      Serial2.print(char(0xcd));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(2) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("GOTO");
      Serial2.print(char(0x47));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("GOTO UP");
      Serial2.print(char(0xc7));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(3) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("North is Down");
      Serial2.print(char(0x4e));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("North is UP");
      Serial2.print(char(0xce));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(4) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("W is Down");
      Serial2.print(char(0x57));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("W is UP");
      Serial2.print(char(0xD7));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(5) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("E is Down");
      Serial2.print(char(0x45));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("E is UP");
      Serial2.print(char(0xc5));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(6) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("S is Down");
      Serial2.print(char(0x53));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("S is UP");
      Serial2.print(char(0xd3));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(7) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("7 is Down");
      Serial2.print(char(0x37));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("7 is UP");
      Serial2.print(char(0xb7));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(8) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("8 is Down");
      Serial2.print(char(0x38));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("8 is UP");
      Serial2.print(char(0xb8));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(9) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("9 is Down");
      Serial2.print(char(0x39));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("9 is UP");
      Serial2.print(char(0xb9));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(10) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("4 is Down");
      Serial2.print(char(0x34));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("4 is UP");
      Serial2.print(char(0xb4));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(11) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("5 is Down");
      Serial2.print(char(0x35));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("5 is UP");
      Serial2.print(char(0xb5));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(12) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("6 is Down");
      Serial2.print(char(0x36));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("6 is UP");
      Serial2.print(char(0xb6));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(13) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("1 is Down");
      Serial2.print(char(0x31));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("1 is UP");
      Serial2.print(char(0xb1));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(14) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("2 is Down");
      Serial2.print(char(0x32));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("2 is UP");
      Serial2.print(char(0xb2));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(15) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("3 is Down");
      Serial2.print(char(0x33));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("3 is UP");
      Serial2.print(char(0xb3));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(16) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("0 is Down");
      Serial2.print(char(0x30));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("0 is UP");
      Serial2.print(char(0xb0));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}

BLYNK_WRITE(17) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("PREV is Down");
      Serial2.print(char(0x2e));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("PREV is UP");
      Serial2.print(char(0xae));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
BLYNK_WRITE(18) // At global scope (not inside of the function)
{
    int i=param.asInt();
    if (i==1) 
    {
      myConsole.println("NEXT is Down");
      Serial2.print(char(0x44));
        //digitalWrite(13, HIGH);
        //digitalWrite(12, LOW);
    }
    else 
    {
      myConsole.println("NEXT is UP");
      Serial2.print(char(0xc4));
        //digitalWrite(12, HIGH);
        //digitalWrite(13, LOW);
    }
}
