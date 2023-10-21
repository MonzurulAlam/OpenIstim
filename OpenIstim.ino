
/***********************************************************************
// 16 June 2023, Australia
// Example code for setting up stimulation current for OpenExoStim v1.0
// For any queries please email RehabExo Pty Ltd at contact@RehabExo.com
***********************************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MCP48xx.h>

// set your stimulation parameters
#define Hz 20      // stimulation frequency
#define pulse_number 10   // number of pulses in a burst
#define pulse_width 50  // pulse width in microsecond
int current = 0;  // declare initial current intesity 
int calibration = 40;  // set intensity calibration value 

// Define the MCP4822 instance, giving it the SS (Slave Select) pin
MCP4822 dac(10);

// Define the SS1306 display connected to I2C pins
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define LED_BLINK 12 // LED to blink during stimulation
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Define rotary encoder pins
#define ENC_A 3
#define ENC_B 2

unsigned int Intensity = 0; // stimulation intensity
unsigned long _lastIncReadTime = micros(); 
unsigned long _lastDecReadTime = micros(); 
int _pauseLength = 2500;
int _fastIncrement = 10;
char buffer[64];  // buffer must be big enough to hold all the message

void setup() {
  pinMode(LED_BLINK, OUTPUT);
  // initialize the DAC
  dac.init();
  // The channels are turned off at startup so we need to turn the channel we need on
  dac.turnOnChannelA();
  dac.turnOnChannelB();
  // We configure the channels in High gain
  // It is also the default value so it is not really needed
  dac.setGainA(MCP4822::High);
  dac.setGainB(MCP4822::High);
  // set channel A to 0 mV
  dac.setVoltageA(0);
  // set channel B to 0 mV
  dac.setVoltageB(0);
  // send the command to the MCP4822
  dac.updateDAC(); 
  // Set encoder pins and attach interrupts
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);
  // Start the serial monitor to show output
  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
}

//**Main program**//
void loop() {
  // generate stimulation pulses
  for (int i=0; i<pulse_number; i++) {
    // positive phase
    // set channel A to set voltage and B to zero
    dac.setVoltageA(Intensity);
    dac.setVoltageB(0);    
    dac.updateDAC();    
    delayMicroseconds(pulse_width);
    // negetive phase
    // set channel A to zero and B to set voltage
    dac.setVoltageA(0);
    dac.setVoltageB(Intensity);
    dac.updateDAC();   
    delayMicroseconds(pulse_width);
  }
  // no pulses 
  //set channel A and B to zero
  dac.setVoltageA(0);
  dac.setVoltageB(0);
  dac.updateDAC(); 
  delay(1000/Hz); // stimulation delay
  digitalWrite(LED_BLINK, !digitalRead(LED_BLINK)); // toggle LED  

  // print the value to serial display
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Calcluate and display set current intesity value
  current = Intensity / calibration;
  sprintf(buffer, "Intensity = %d mA", current);
  display.println(buffer);
  display.display();
  //Intensity = Intensity + 1;   // for debugging only
}

//**Interrupt service routine**//
void read_encoder() {
  // Encoder interrupt routine for both pins. Updates Intensity
  // if they are valid and have rotated a full indent
 
  static uint8_t old_AB = 3;  // Lookup table index
  static int8_t encval = 0;   // Encoder value  
  static const int8_t enc_states[]  = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Lookup table

  old_AB <<=2;  // Remember previous state

  if (digitalRead(ENC_A)) old_AB |= 0x02; // Add current state of pin A
  if (digitalRead(ENC_B)) old_AB |= 0x01; // Add current state of pin B
  
  encval += enc_states[( old_AB & 0x0f )];

  // Update Intensity if encoder has rotated a full indent, that is at least 4 steps
  if( encval > 3 ) {        // Four steps forward
    int changevalue = 10;
    if((micros() - _lastIncReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastIncReadTime = micros();
    Intensity = Intensity + changevalue;              // Update Intensity
    encval = 0;
  }
  else if( encval < -3 ) {  // Four steps backward
    int changevalue = -10;
    if((micros() - _lastDecReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastDecReadTime = micros();
    if (Intensity > 0){
      Intensity = Intensity + changevalue;              // Update Intensity
    }
    else{
      Intensity = 0;
    }
    encval = 0;
  }
}
