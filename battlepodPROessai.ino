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


//#define DEBUG_RUMBLE //uncomment this line to trigger rumble by pressing any button

#define ANALOG_ACCEL //uncomment this line to activate analog acceleration
#define ANALOG_BRAKE //uncomment this line to activate analog brak
#define SINGLE_RUMBLE //comment this line if you have two separate rumble motorse

#define INPUTS 12

//Set these at your actual hardware phisical limits
#define STEER_MIN 265
#define STEER_MAX 815
#define ACCEL_MIN 295
#define ACCEL_MAX 815
#define BRAKE_MIN 150
#define BRAKE_MAX 873

//#define RUMBLE_WATCHDOG //Comment THIS line to disable watchdog routines
#define RUMBLE_TIMEOUT 30000 //time (in milliseconds) after which rumble is force-disabled (watchdog)

unsigned long rumbleTime;
unsigned long rumbleHoldTime;
boolean rumbling; //rumble flag
boolean quieting; //rumble quiet flag
boolean PANIC; //panic flag

const int rumble1Pin = 9; //RUMBLE 1 OUT
const int rumble2Pin = 10; //RUMBLE 2 OUT
const int wheelPin = A0; //WHEEL
const int accelPin = A1; //ACCELERATOR
const int brakePin = A2; //BRAKE
const int panicPin = A3; //FORCE RUMBLE DISABLE/ PANIC

int bigRumble;
int smallRumble;
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
  {15, HIGH, 0, BUTTON_A, BUTTON_L3},          //1P_B2
  {16, HIGH, 0, BUTTON_Y, BUTTON_R3},         //1P_B3
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
  rumbling = false;
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

void RumbleHandling(){
  bigRumble = XInput.getRumbleLeft();
  smallRumble = XInput.getRumbleRight();
  //uint16_t bothRumble = XInput.getRumble();

  #ifdef RUMBLE_WATCHDOG
    if(bigRumble > 0 || smallRumble > 0){
      rumbleHoldTime = millis();//reset the rumble quiet phase time as soon as a rumble event is triggered
      if(rumbling == false){
        rumbling = true;
        rumbleTime = millis();
      }
      if(millis() - rumbleTime > RUMBLE_TIMEOUT){//kill rumble after RUMBLE_TIMEOUT (watchdog)
        rumbleEnable = false;
        digitalWrite(rumble1Pin, LOW);
        digitalWrite(rumble2Pin, LOW);  
      }
    }
    if(bigRumble == 0 && smallRumble == 0){
      if(rumbleEnable == false && quieting == false){
        quieting = true;
      }
      if(millis() - rumbleHoldTime > 1000){
        rumbleEnable = true;
        quieting = false;
        rumbling = false;
      }
    }
  #endif  
  if(PANIC == 0){
       
      #ifdef SINGLE_RUMBLE
      if(rumbleEnable)
       {
        
//       if(bigRumble >= smallRumble)
//          {
//         analogWrite(rumble1Pin, bigRumble);
//          //analogWrite(rumble2Pin, bigRumble);
//        }
//        else{
//         analogWrite(rumble1Pin, smallRumble);
//         //analogWrite(rumble2Pin, smallRumble);
//       }
//        }
//    }
//    #else
      
//      if(rumbleEnable){
        analogWrite(rumble1Pin, bigRumble);
        analogWrite(rumble2Pin, smallRumble);
      }
      }
    #endif
  else{//PANIC!
    digitalWrite(rumble1Pin, LOW);
    digitalWrite(rumble2Pin, LOW);  
  }
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
  //aValue = map(aValue, ACCEL_MIN, ACCEL_MAX, 0, 1023);//EXTEND THE RANGE
  if(aValue > 515 || aValue < 509){ //very little deadzone (+/-3)
    aValue = map(aValue, ACCEL_MIN, ACCEL_MAX, 0, 1023);
    if(aValue < 0){
    aValue = 0;
    }
     else if (aValue > 1023){
    aValue = 1023;
    
  }
  XInput.setJoystickY(JOY_LEFT, aValue); //LEFT STICK, Y-AXIS
}
}

void BrakeHandling(){
  bValue = analogRead(brakePin);
  //by default, X, Y axes, 16-bit signed values (-32768 is min, 32767 is max, and 0 is centered)
  //Function XInput.setJoystickRange(min, max) force this to a custom range
   if(bValue > 515 || bValue < 509){ //very little deadzone (+/-3)
    bValue = map(bValue, BRAKE_MIN, BRAKE_MAX, 0, 1023);
    if(bValue < 0){
    bValue = 0;
    }
     else if (bValue > 1023){
    bValue = 1023;
   }
  XInput.setJoystickY(JOY_RIGHT, bValue); //RIGHT STICK, Y-AXIS
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
