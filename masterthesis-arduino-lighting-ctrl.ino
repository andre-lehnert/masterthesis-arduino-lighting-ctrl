/* --------------------------------------------------------------------
This file is part of the master thesis "A Shape-Changing Display for 
Ambient Notifications".

see http://andre-lehnert.de/dokuwiki/doku.php?id=wiki:publikationen:masterarbeit:implementierung:i2c

This Sketch is used to control a LED stripe with 44 WS2812B RGB LEDs.
- An Arduino Nano is adressed by I2C https://www.arduino.cc/en/Reference/Wire
  see "I2C Adressing Schema"
- A Raspberry Pi 3 works as I2C Master

Included libraries:
- Arduino Wire
- Arduino Animation Library (modified)
- Adafruit NeoPixel (WS2812B)
- Debugging Tools

The Arduino ALA library is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

The Arduino ALA library is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with The Arduino ALA library.  If not, see <http://www.gnu.org/licenses/>.

--------------------------------------------------------------------
I2C Adressing Schema
--------------------------------------------------------------------

- Columns: A, B, C, ...
- Rows: 1, 2, 3, ...
- Bar adressing: A1, B2, C3, ...

I2C Master
- 0x0f

Bar/ I2C Receiver
- Motor Controller Adress: even numbers
- LED Controller Adress: odd numbers

- e.g. Row 1 = 0x1#: 
  - Bar A1: 0x10 (Motor), 0x11 (LED)
  - Bar B1: 0x12 (Motor), 0x13 (LED)
  - Bar C1: 0x14 (Motor), 0x15 (LED)
  
- e.g. Bar A1:

        [A   ][B   ][C   ][D   ][E   ][F   ][       ][RPI]
        0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
[  ] 00:          -- -- -- -- -- -- -- -- -- -- -- -- 0f 
[ 1] 10: 10 11 -- -- -- -- -- -- -- -- -- -- -- -- -- --
[ 2] 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
[ 3] 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
[ 4] 40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
[ 5] 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
[ 6] 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
[  ] 70: -- -- -- -- -- -- -- --
-------------------------------------------------------------------- */

#include <Wire.h>               // I2C https://www.arduino.cc/en/Tutorial/MasterWriter
#include <AlaLedLite.h>         // modified Arduino Animation Library http://yaab-arduino.blogspot.de/p/ala.html
#include <Debugger.h>           // Debugging Tool

////////////////////////////////////////////////////////////////////////////////

#define MASTER_ADDRESS  0x0f    // Raspberry Pi, Decimal: 15
#define SLAVE_ADDRESS   0x11    // MotorController 0x10 + 1, Decimal: 17

#define LED_PIN         8       // D8

#define BAUD_RATE       9600    // Serial Baud Rate
#define I2C_STATUS_PIN  2       // D2

////////////////////////////////////////////////////////////////////////////////
// Parameter

AlaLedLite led;                   // LED Stripe
Debugger debug;                   // Debugger

boolean _serialActive   = false;  // MotorController recognized i2c-jumper
boolean _i2cActive      = false;

boolean _setupComplete  = false;
long _ts                = 0;

////////////////////////////////////////////////////////////////////////////////

void setup() {
  
  // Start HW Debugging
  pinMode(13, OUTPUT); 
  
  // Start Serial/ Debugging
  debug.enable(BAUD_RATE);      // Baudrate 9600
  _serialActive = true;

  // Wait for MotorController
  delay(1000);
  
  if ( isI2CDeactivated() ) {

    debug.println("//// ----- Serial Mode ----- ////");
       
  } else {

    // Start I2C
    Wire.begin(SLAVE_ADDRESS);
    Wire.onReceive(receiveEvent); // register event handler
  
    debug.println("//// ----- I2C Mode ----- ////");
    debug.println("MASTER ID: ", MASTER_ADDRESS);
    debug.println("OWN ID: ", SLAVE_ADDRESS); 
    debug.println("//// -------------------- ////");
    
  }

  // Start LED Stripe
  led.initBarStripes(LED_PIN); 
}

////////////////////////////////////////////////////////////////////////////////

void loop() {  

  // Check Serial Port
  if ( ! _i2cActive ) { // i2c deactivated
    checkSerial();   
  } 

  // Perfom animations:
  // 1. setAnimation
  // 2. runAnimation
  led.runAnimation();
}

////////////////////////////////////////////////////////////////////////////////

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int h) {
  
  debug.println("//// -------------------- ////");
  debug.println("Receiving I2C Message:");
  
  digitalWrite(13, HIGH);

  // Get message
  String msg = "";
  while(1 <= Wire.available()) {  
    char c =  Wire.read(); 
    msg += String( c );     
  }

  debug.print(msg);

  parseMessage(msg);

  digitalWrite(13, LOW);
}


void checkSerial() {

  if (Serial.available() > 0) {
    
    debug.println("//// -------------------- ////");
    debug.println("Receiving Serial Message:");   

    digitalWrite(13, HIGH);

    // Get message
    String msg = ""; 

    msg += Serial.readString();  
  
    debug.print(msg);
  
    parseMessage(msg);

    digitalWrite(13, LOW);
  }
}

void parseMessage(String msg) {

  debug.println("Parsing Message...");
  debug.println(msg);

  // Example
  // [COMMAND]:[ANIMATION]/[COLOR HEX]/[BRIGHTNESS 0-100]/[SPEED 0-100]

  // 1) Seperate command and value
  char valueSeperator = ':';
  
  String temp = msg;
  int maxIndex = temp.length()-1;  
  String cmd = "";
  String value = "";
  
  for (int i = 0; i < maxIndex; i++) {
    if( temp.charAt(i) == valueSeperator ) {
      cmd = temp.substring(0, i);
      value = temp.substring(i+1, maxIndex+1);
      
      debug.println("Command: ", cmd);      
      debug.println("Value: ", value);
    }
  }

  // 2) Execute commands
  if (cmd.indexOf("LIGHT") > -1) {
    
    debug.println("Lighting:");

    // Parse LIGHT parameter
    const int paramNum = 5;
    String lighting[paramNum];    
    int rgba[4] = { 0, 0, 0, -1};
    int lastEntry = -2;
    
    String tempValue = value;
        
    // Parse Value
    for (int param = 0; param < paramNum+1; param++) {

        if (tempValue.indexOf("/") > -1 ) {          
          
          lighting[param] = tempValue.substring(0, tempValue.indexOf("/") );
          debug.println(String( param + 1) + ": \t", lighting[param] );  
                    
          tempValue = tempValue.substring(tempValue.indexOf("/")+1, tempValue.length());
          lastEntry = param; // LIGHT:A/+/ff1464/100 <- last
          
        } else if (param == lastEntry + 1) { 
          lighting[param] = tempValue;   
          debug.println(String( param + 1) + ": \t", lighting[param] );             
        }  
    }

    String side = lighting[0];
    debug.println("Side:", side);

    String op = lighting[1];
    debug.println("Operation:", op);

    int led = lighting[2].toInt();
    debug.println("LED Number:", led);

    // Convert HEX to RGB
    if ( op != "-" ) {
      rgba[0] = hexToDec(lighting[3].substring(0,2));
      rgba[1] = hexToDec(lighting[3].substring(2,4));
      rgba[2] = hexToDec(lighting[3].substring(4,6));    
      rgba[3] = lighting[4].toInt();      
      debug.println("R:", rgba[0]);
      debug.print("G:", rgba[1]);
      debug.print("B:", rgba[2]);
      debug.print("Brightness:", rgba[3]);
    }

    // Update Lighting Pattern
    setLighting(side, op, led, rgba);
    
  } else if (cmd.indexOf("ANI") > -1) {
    
    debug.println("Animations:");

    // Parse LIGHT parameter
    const int paramNum = 4;
    String lighting[paramNum];    
    int rgba[4];    
    int lastEntry = -2;
    
    String tempValue = value;
        
    // Parse Value
    for (int param = 0; param < paramNum+1; param++) {

        if (tempValue.indexOf("/") > -1 ) {          
          
          lighting[param] = tempValue.substring(0, tempValue.indexOf("/") );
          debug.println(String( param + 1) + ": \t", lighting[param] );  
                    
          tempValue = tempValue.substring(tempValue.indexOf("/")+1, tempValue.length());
          lastEntry = param; // LIGHT:mov/ff1464/100/100 <- last
          
        } else if (param == lastEntry + 1) { 
          lighting[param] = tempValue;   
          debug.println(String( param + 1) + ": \t", lighting[param] );             
        }  
    }
    
    // Convert HEX to RGB
    rgba[0] = hexToDec(lighting[1].substring(0,2));
    rgba[1] = hexToDec(lighting[1].substring(2,4));
    rgba[2] = hexToDec(lighting[1].substring(4,6));
    rgba[3] = lighting[2].toInt();
    debug.println("R:", rgba[0]);
    debug.print("G:", rgba[1]);
    debug.print("B:", rgba[2]);
    debug.print("Brightness:", rgba[3]);    

    
    int spd = lighting[3].toInt();
    debug.println("Animation Speed:", lighting[3]);

    // Update Animation
    setAnimation(lighting[0], rgba, spd);
  }
}

/* -------------------------------------------------------------------------------------
 * Animations
 */

void setLighting(String side, String operation, int num, int rgba[4]) {

  // Scale and set brightness
  if (rgba[3] >= 0) {
    int brightness = getScaledBrightness(rgba[3]);
    led.setBrightness(AlaColor(brightness, brightness, brightness)); // TODO
  }
  
  if (operation == "+") {
    
    led.setLED(side, num, ALA_ON, AlaColor(rgba[0], rgba[1], rgba[2]));
  
  } else if (operation == "-") {
    
    led.setLED(side, num, ALA_OFF, AlaColor(0, 0, 0));
  
  } else if (operation == "*") {
    
    led.setSide(side, ALA_OFF, AlaColor(0, 0, 0));
    //led.setLED(side, num, ALA_ON, AlaColor(rgba[0], rgba[1], rgba[2]));
  }

  led.setCustomLighting(true);
}

void setAnimation(String animation, int rgba[4], int spd) {

  // Scale and set brightness
  int brightness = getScaledBrightness(rgba[3]);
  led.setBrightness(AlaColor(brightness, brightness, brightness));

  // Special duration-speed mapping for each animation
  int duration;
  
  if (animation == "off") {
    
    led.setAnimation(ALA_OFF, 0, AlaColor(0, 0, 0));
     
  } else if (animation == "on") {
    
    led.setAnimation(ALA_ON, 0, AlaColor(rgba[0], rgba[1], rgba[2]));
     
  } else if (animation == "glo") {
  
    duration = map(spd, 0, 100, 20000, 1000);
    led.setAnimation(ALA_GLOW, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
     
  } else if (animation == "bli") {
  
    duration = map(spd, 0, 100, 4000, 200);
    led.setAnimation(ALA_BLINK, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
     
  } else if (animation == "up") {
  
    duration = map(spd, 0, 100, 10000, 800);
    led.setAnimation(ALA_PIXELSMOOTHSHIFTRIGHT, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
     
  } else if (animation == "dow") {
    
    duration = map(spd, 0, 100, 10000, 800);
    led.setAnimation(ALA_PIXELSMOOTHSHIFTLEFT, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
    
  } else if (animation == "com") {
    
    duration = map(spd, 0, 100, 10000, 800);
    led.setAnimation(ALA_COMET, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
    
  } else if (animation == "bou") {
    
    duration = map(spd, 0, 100, 10000, 800);
    led.setAnimation(ALA_PIXELSMOOTHBOUNCE, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
     
  } else if (animation == "mov") {
  
    duration = map(spd, 0, 100, 10000, 800);
    led.setAnimation(ALA_LARSONSCANNER, duration, AlaColor(rgba[0], rgba[1], rgba[2]));
  }

  debug.println("//// -------------------- ////");
  debug.println("Animation: ", animation);
  debug.println("Color: ", rgba[0]);
    debug.print(",", rgba[1]);
    debug.print(",", rgba[2]);
  debug.println("Brightness: ", rgba[3]);
  debug.println("Speed: ", spd);
  debug.println("Duration: ", duration);
  debug.println("//// -------------------- ////");

  led.setCustomLighting(false);
}

/* -------------------------------------------------------------------------------------
 * 
 */

// Scale brightness values 
// 0 -> 0
// 100 -> 255
int getScaledBrightness(int percent) {
  return map(percent, 0, 100, 0, 255);
}

// Convert color HEX #FF to RGB 255
unsigned int hexToDec(String hexString) {
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}

// Unidirectional bitwise communication between 
// the MotorController (D2 OUT) and the LEDController (D2 IN) 
// 1 = Serial Communication
// 0 = I2C Communication
boolean isI2CDeactivated() {
  
  pinMode(I2C_STATUS_PIN, INPUT);  
   
  int value;
  value = digitalRead(I2C_STATUS_PIN);  

  _i2cActive = (value == 0);  
  
  return (value == 1);
}

boolean isValidNumber(String str){
   for(byte i=0;i<str.length();i++)
   {
      if( ! isDigit(str.charAt(i)) ) {        
        return false;
      }
   }
   return true;
} 
