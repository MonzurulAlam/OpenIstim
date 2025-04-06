
/***********************************************************************
// 16 June 2023, Australia
// Example code for setting up stimulation current for OpenXatim2 v1.0
// For queries please email Dr Monzurul Alam at md.malam@connect.polyu.hk
***********************************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>
#include <AD57.h> 

// set your stimulation parameters
#define pulse_number 10   // number of pulses in a burst
#define pulse_width 10  // calibrated pulse width in microsecond
int Hz = 20;      // decleare initial stimulation frequency
int current = 0;  // declare initial current intesity 
int calibration = 10;  // set intensity calibration value 
const int Push_BUTTON = 4; // Rotary encoder push button
byte lastButtonState = LOW;
unsigned long debounceDuration = 100; // millis
unsigned long lastTimeButtonStateChanged = 0;

/******************************************************
 * user variables
 *****************************************************/
// MUST : select the kind of AD57xx you have:
// AD5722 or AD5732 or AD5752 
uint8_t DacType = AD5722;

// MUST : select the DAC channel to use (DAC_A, DAC_B or DAC_AB)
#define DAC_CHANNEL DAC_AB

/* MUST : define the output voltage range (for Bipolar use pnxxx)
 * p5V          +5V
 * p10V         +10V
 * p108V        +10.8V
 * pn5V         +/-5V
 * pn10V        +/-10V
 * pn108V       +/-10.8V
 */
#define output_range pn5V

// MUST : DEFINE THE SYNC PIN CONNECTION FOR AD57xx
/* The initialisation and pulling high and low is handled by the 
 * library. The sketch only needs to define the right pin.
 */
char slave_pin_DAC = 9;

// Define the SS1306 display connected to I2C pins
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define LED_BLINK 12 // LED to blink during stimulation
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Define rotary encoder pins
#define ENC_A 3
#define ENC_B 2

int16_t initial_value;
int16_t baseline = 2048;
int16_t value = baseline;
unsigned int Counter = 0; 
unsigned long _lastIncReadTime = micros(); 
unsigned long _lastDecReadTime = micros(); 
int _pauseLength = 2500;
int _fastIncrement = 1;
char buffer[64];  // buffer must be big enough to hold all the message

void setup() {
  pinMode(LED_BLINK, OUTPUT);
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
  //serialTrigger(F("Press any key to begin +5V to -5V  Bi-polar constant loop output range mode"));  
  /* enable or disable debug information from the library
   *  0 = disable (default.. no need to call)
   *  1 = enable
   */
  AD57.debug(0);
  Serial.println(F("Starting DAC"));
  // initialise the AD57xx and connections to and
  // from the AD57xx
  init_AD57xx();
  // set max count depending on AD57xx : 12, 14 or 16 bits
  if (DacType == AD5752)    initial_value = 0xffff;
  else if (DacType == AD5732) initial_value = 0x3fff;
  else if (DacType == AD5722) initial_value = 0x0fff;
}

//**Main program**//
void loop() {
  value = baseline + Counter;
  // generate stimulation pulses
  for (int i=0; i<pulse_number; i++) {
    // positive phase
    AD57.setDac(DAC_CHANNEL, value);
    delayMicroseconds(pulse_width);
    // negetive phase
    AD57.setDac(DAC_CHANNEL, -value);
    delayMicroseconds(pulse_width);
  }
  // no pulses 
  AD57.setDac(DAC_CHANNEL, baseline);
  //delayMicroseconds(pulse_width);
  delay(700/Hz); // stimulation delay
  //digitalWrite(LED_BLINK, !digitalRead(LED_BLINK)); // toggle LED  

  // print the value to serial display
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Calcluate and display set current intesity value
  current = Counter / calibration;
  sprintf(buffer, "Current = %d mA", current);
  display.println(buffer);
  display.display();

  // input stimulation parameters if the button is pressed
  if (millis() - lastTimeButtonStateChanged > debounceDuration) {
     byte buttonState = digitalRead(Push_BUTTON);
     if (buttonState != lastButtonState) {
       lastTimeButtonStateChanged = millis();
       lastButtonState = buttonState;
       if (buttonState == LOW) {
        Counter = Hz;
        digitalWrite(LED_BLINK, LOW); // LED ON
        while(1){
          Hz = Counter;
          // print the value to serial display
          display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(WHITE);
          display.setCursor(0, 0);
          sprintf(buffer, "Frequency = %d Hz", Counter);
          display.println(buffer);
          display.display();
          delay(200);// do something
          if(digitalRead(Push_BUTTON)==0) break;
        }
        Counter = 0;
       }
     }
  }
  //Counter = Counter + 1;   // for debugging only
}

//**Interrupt service routine**//
void read_encoder() {
  // Encoder interrupt routine for both pins. Updates Counter
  // if they are valid and have rotated a full indent
 
  static uint8_t old_AB = 3;  // Lookup table index
  static int8_t encval = 0;   // Encoder value  
  static const int8_t enc_states[]  = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Lookup table

  old_AB <<=2;  // Remember previous state

  if (digitalRead(ENC_A)) old_AB |= 0x02; // Add current state of pin A
  if (digitalRead(ENC_B)) old_AB |= 0x01; // Add current state of pin B
  
  encval += enc_states[( old_AB & 0x0f )];

  // Update Counter if encoder has rotated a full indent, that is at least 4 steps
  if( encval > 3 ) {        // Four steps forward
    int changevalue = 5;
    if((micros() - _lastIncReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastIncReadTime = micros();
    Counter = Counter + changevalue;              // Update Counter
    encval = 0;
  }
  else if( encval < -3 ) {  // Four steps backward
    int changevalue = -5;
    if((micros() - _lastDecReadTime) < _pauseLength) {
      changevalue = _fastIncrement * changevalue; 
    }
    _lastDecReadTime = micros();
    if (Counter > 0){
      Counter = Counter + changevalue;              // Update Counter
    }
    else{
      Counter = 0;
    }
    encval = 0;
  }
}

/* initialize the connection with the board  and set the default values */
void init_AD57xx()
{
  // set output voltage range in binary
  // if you want to test 2scomp, see remark 1 in AD57.h, use
  // AD57.begin(DAC_CHANNEL, output_range, COMP2); 
  AD57.begin(DAC_CHANNEL, output_range);

  Serial.print("get range ");
  Serial.println(AD57.getRange(DAC_CHANNEL));

  // enable therminal shutdown and overcurrent protection
  // also stop CLR as, starting from 0v after OP_CLEAR op_code)
  // this can be changed for full negative after clear with
  // AD57.setControl(OP_INSTR,(SET_TSD_ENABLE |SET_CLAMP_ENABLE|SET_CLR_SET));
  AD57.setControl(OP_INSTR,(SET_TSD_ENABLE |SET_CLAMP_ENABLE|STOP_CLR_SET));
   
  // clear the DAC outputs
  AD57.setControl(OP_CLEAR);
}

bool check_status(bool disp)
{
    uint16_t stat = AD57.getStatus();
    bool ret = 0;
    
    if (stat & stat_err_TO)
    {
      Serial.println(F("Therminal overheat shutdown"));
      ret = 1;
    }
    
    if (stat & stat_err_CA)
    {
      Serial.println(F("DAC - A overcurrent detected"));
      ret = 1;
    }

    if (stat & stat_err_CB)
    {
      Serial.println(F("DAC - B overcurrent detected"));
      ret = 1;
    }

    if (disp == 1)
    {
      Serial.println(F("Display settings\n"));
      
      if (stat & stat_TS)
         Serial.println(F("Therminal shutdown protection enabled"));
      else
         Serial.println(F("Therminal shutdown protection disabled"));

      if (stat & stat_CLR)
        Serial.println(F("DAC output set to midscale or fully negative with OP_CLEAR command"));
      else
        Serial.println(F("DAC output set to 0V with OP_CLEAR command"));
      
      if (stat & stat_CLAMP)
         Serial.println(F("Overcurrent protection enabled"));
      else
         Serial.println(F("Overcurrent protection disabled"));

      if (stat & stat_SDO) // you will never see this one :-)
         Serial.println(F("Serial data out disabled"));
      else          
         Serial.println(F("Serial data out enabled"));

      Serial.println();
    }

    return(ret);
}

/* serialTrigger prints a message, then waits for something
 * to come in from the serial port.
 */
void serialTrigger(String message)
{
  Serial.println();
  Serial.println(message);
  Serial.println();

  while (!Serial.available());

  while (Serial.available())
    Serial.read();
}

/* errorLoop loops forever.*/
void errorLoop()
{
  // power-off both channels 
  AD57.setPower(STOP_PUA|STOP_PUB);
  
  Serial.print(F("Critical error, looping forever"));
  for (;;);
}
