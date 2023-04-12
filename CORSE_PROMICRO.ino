/* CORSE - PC to JAMMA interface board for generic arcade racing cabinets.
 *  
 * Arduino promicro - XInput emulation/Ruble management
 * 
 * Detailed instructions here:
 * https://www.partsnotincluded.com/how-to-emulate-an-xbox-controller-with-arduino-xinput/
 * 
 * To upload XInput sketches, select "arduino Leonardo w/ XInput", not "arduino Leonardo".
 * After the first sketch has been uploaded, the COM port will be not selectable; upload the new sketch anyway, and reset the board manually.
 * 
 * by barito 2021-2023 (last update apr 2023)
 */

#include <XInput.h>

#define SINGLE_RUMBLE //comment this line if you have two separate rumble motors
//#define DEBUG_RUMBLE //uncomment this line to trigger rumble by pressing any button

//#define ANALOG_ACCEL //uncomment this line to activate analog acceleration
//#define ANALOG_BRAKE //uncomment this line to activate analog brake

#define INPUTS 12

//Set these at your actual hardware phisical limits
#define STEER_MIN 150
#define STEER_MAX 873
#define ACCEL_MIN 300
#define ACCEL_MAX 600
#define BRAKE_MIN 300
#define BRAKE_MAX 600

#define RUMBLE_TIMEOUT 2000 //time (in milliseconds) after which rumble is force-disabled

unsigned long rumbleTime;
boolean PANIC;

const int rumble1Pin = 15; //RUMBLE 1 OUT
const int rumble2Pin = 16; //RUMBLE 2 OUT
const int wheelPin = A0; //WHEEL
const int accelPin = A1; //ACCELERATOR
const int brakePin = A2; //BRAKE
const int panicPin = A3; //FORCE RUMBLE DISABLE/ PANIC

uint8_t bigRumble;
uint8_t smallRumble;
bool startBlock = 0;
const int delayTime = 20;
int wValue;
int aValue;
int bValue;
bool rumbleEnable;

unsigned long panicDB;
boolean panicState;

const unsigned long noLossDel = 8;

// Xinput library input configuration:
// 0 -10 -> buttons (BUTTON_LOGO, BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y, BUTTON_LB, BUTTON_RB, BUTTON_BACK, BUTTON_START, BUTTON_L3, BUTTON_R3)
// 11-14 -> cross hatch (DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT)
// Set buttons at your will, but prefer "BUTTONS" on pins you will use: some software has issues with DPADS (often seen as POV).
struct digitalInput {const byte pin; boolean state; unsigned long dbTime; const byte btn; const byte btn_shift;} 
digitalInput[INPUTS] = {
  {3, HIGH, 0, BUTTON_START, BUTTON_START},   //1P_START - SHIFT BUTTON
  {2, HIGH, 0, BUTTON_BACK, BUTTON_BACK},     //1P_COIN
  {0, HIGH, 0, DPAD_RIGHT, DPAD_LEFT},        //SERVICE
  {1, HIGH, 0, DPAD_LEFT, DPAD_RIGHT},        //TEST
  {4, HIGH, 0, BUTTON_RB, DPAD_UP},           //1P_UP
  {5, HIGH, 0, BUTTON_LB, DPAD_DOWN},         //1P_DOWN
  {6, HIGH, 0, DPAD_LEFT, DPAD_LEFT},         //1P_LEFT
  {7, HIGH, 0, DPAD_RIGHT, DPAD_RIGHT},       //1P_RIGHT
  {8, HIGH, 0, BUTTON_X, BUTTON_B},           //1P_B1
  {9, HIGH, 0, BUTTON_A, BUTTON_L3},          //1P_B2
  {10, HIGH, 0, BUTTON_Y, BUTTON_R3},         //1P_B3
  {14, HIGH, 0, DPAD_RIGHT, DPAD_RIGHT}       //1P_B4
};

void setup() {
  for (int j = 0; j < INPUTS; j++){
    pinMode(digitalInput[j].pin, INPUT_PULLUP);
    digitalInput[j].state = digitalRead(digitalInput[j].pin);
    digitalInput[j].dbTime = millis();
  }
  pinMode(wheelPin, INPUT_PULLUP);
  pinMode(accelPin, INPUT_PULLUP);
  pinMode(brakePin, INPUT_PULLUP);
  pinMode(panicPin, INPUT_PULLUP);
  pinMode(rumble1Pin, OUTPUT);
  pinMode(rumble2Pin, OUTPUT);
  PANIC = false;
  rumbleEnable = true;
  XInput.begin();
  XInput.setJoystickRange(0, 1023); //by Xinput lib. default, X/Y axes are 16-bit signed values (-32768 is min, 32767 is max, 0 is centered)
}

void loop() {
  RumbleHandling();
  WheelHandling();
  #ifdef ANALOG_ACCEL
    AccelHandling();
  #endif
  #ifdef ANALOG_BRAKE
    BrakeHandling();
  #endif
  GeneralInputs();
  ShiftInputs();
  #ifdef DEBUG_RUMBLE
    RumbleDebug();
  #endif
}

void WheelHandling(){
  wValue = analogRead(wheelPin);
  //by default, X, Y axes, 16-bit signed values (-32768 is min, 32767 is max, and 0 is centered)
  //Function XInput.setJoystickRange(min, max) force this to a custom range
  if(wValue > 515 || wValue < 509){ //very little deadzone (+/-3)
    wValue = map(wValue, STEER_MIN, STEER_MAX, 0, 1023);
    if(wValue < 0){
      wValue = 0;
    }
    else if (wValue > 1023){
      wValue = 1023;
    }
    XInput.setJoystickX(JOY_LEFT, wValue); //LEFT STICK, X-AXIS
  }
}

void AccelHandling(){
  aValue = analogRead(accelPin);
  //by default, X, Y axes, 16-bit signed values (-32768 is min, 32767 is max, and 0 is centered)
  //Function XInput.setJoystickRange(min, max) force this to a custom range
  aValue = map(aValue, ACCEL_MIN, ACCEL_MAX, 0, 1023);//EXTEND THE RANGE
  if(aValue < 0){
    aValue = 0;
  }
  else if (aValue > 1023){
    aValue = 1023;
  }
  aValue = aValue>>2;//Xinput lib triggers are 8-bit unsigned
  XInput.setTrigger(TRIGGER_RIGHT, aValue); //LEFT STICK, Y-AXIS
}

void BrakeHandling(){
  bValue = analogRead(brakePin);
  //by default, X, Y axes, 16-bit signed values (-32768 is min, 32767 is max, and 0 is centered)
  //Function XInput.setJoystickRange(min, max) force this to a custom range
  bValue = map(bValue, BRAKE_MIN, BRAKE_MAX, 0, 1023);
  if(bValue < 0){
    bValue = 0;
  }
  else if (bValue > 1023){
    bValue = 1023;
  }
  bValue = bValue>>2;//Xinput lib triggers are 8-bit unsigned
  XInput.setTrigger(TRIGGER_LEFT, bValue); //RIGHT STICK, Y-AXIS
}

void RumbleHandling(){
  bigRumble = XInput.getRumbleLeft();
  smallRumble = XInput.getRumbleRight();
  //uint16_t bothRumble = XInput.getRumble();
  if(millis() - rumbleTime > RUMBLE_TIMEOUT){
    rumbleEnable = false;
  }
  if(PANIC == 0){
    #ifdef SINGLE_RUMBLE
    if(bigRumble == 0 && smallRumble == 0){
      digitalWrite(rumble1Pin, LOW);
      digitalWrite(rumble2Pin, LOW);
      rumbleEnable = true;
      rumbleTime = millis();
    }
    else{
      if(rumbleEnable){
        if(bigRumble > smallRumble){
          analogWrite(rumble1Pin, bigRumble);
          analogWrite(rumble2Pin, bigRumble);
        }
        else{
          analogWrite(rumble1Pin, smallRumble);
          analogWrite(rumble2Pin, smallRumble);
        }
      }
    }
    #else
    if(bigRumble == 0 && smallRumble == 0){
      rumbleEnable = true;
      rumbleTime = millis();
    }
    if(bigRumble == 0){
      digitalWrite(rumble1Pin, LOW);
    }
    else{
      if(rumbleEnable){
        analogWrite(rumble1Pin, bigRumble);
      }
    }
    if(smallRumble == 0){
      digitalWrite(rumble2Pin, LOW);
    }
    else{
      if(rumbleEnable){
        analogWrite(rumble2Pin, smallRumble);
      }
    }
    #endif
  }
  else{//PANC!
    digitalWrite(rumble1Pin, LOW);
    digitalWrite(rumble2Pin, LOW);  
  }
}

void GeneralInputs(){
//general input handling
  for (int j = 1; j < INPUTS; j++){
   if (millis()-digitalInput[j].dbTime > delayTime && digitalRead(digitalInput[j].pin) !=  digitalInput[j].state){
      digitalInput[j].state = !digitalInput[j].state;
      digitalInput[j].dbTime = millis();
      if(digitalInput[0].state == HIGH){ //shift button not pressed
         XInput.setButton(digitalInput[j].btn, !digitalInput[j].state);
         delay(noLossDel);
      }
      else{ //shift button pressed
         startBlock = 1;
         XInput.setButton(digitalInput[j].btn_shift, !digitalInput[j].state);
         delay(noLossDel);
      }
    }
  }
  if (millis()- panicDB > 400 /*long enought delay time*/ && digitalRead(panicPin) !=  panicState){
    panicState = !panicState;
    panicDB = millis();
    if(panicState == LOW){//PANIC button is pressed
       PANIC = !PANIC; //toggle rumble enable
    }
  }
}

void ShiftInputs(){
//reversed input handling - shift button
  if (millis()-digitalInput[0].dbTime > delayTime && digitalRead(digitalInput[0].pin) !=  digitalInput[0].state){
    digitalInput[0].state = !digitalInput[0].state;
    digitalInput[0].dbTime = millis();
    if (digitalInput[0].state == HIGH){
      if(startBlock == 0){
        XInput.setButton(digitalInput[0].btn, HIGH);
        delay(100);
        XInput.setButton(digitalInput[0].btn, LOW);
        delay(70);
       }
      else{
        startBlock = 0;
      }
      _RelAllSft();//release all shift buttons (if you release shift before the button, the shift button stay pressed)
    }
  }
}

void _RelAllSft(){//release all shift buttons (if you release start/shift before the button, the shift button stay pressed)
  for (int j = 1; j < INPUTS; j++){
    XInput.setButton(digitalInput[j].btn_shift, false);
    delay(noLossDel);
   }
}

void RumbleDebug(){
  for (int j = 0; j < INPUTS; j++){
    if (digitalInput[j].state == LOW){
      digitalWrite(rumble1Pin, HIGH);
      digitalWrite(rumble2Pin, HIGH);
    }
    else{
      digitalWrite(rumble1Pin, LOW);
      digitalWrite(rumble2Pin, LOW);
    }
  }
}
