/*
CORSE - PC to JAMMA interface board for generic arcade racing cabinets.
Arduino NANO - Mamehooker outputs read + frequency block sketch
This sketch reads outputs signals from Howard Casto's Mamehooker, computes sync frequency 
and disables video amp when frequencies are higher than CGA
Here is an example of mamehooker .ini code:
(messages sent to Arduino serial port COM4)
[General] 
MameStart=cmo 4 baud=9600_parity=N_data=8_stop=1 
MameStop=cmc 4 
StateChange= 
OnRotate= 
OnPause= 
[KeyStates] 
RefreshTime= 
[Output] 
lamp0=
lamp1=cmw 4 1., cmw 4 %s%, cmw 4 x
lamp2=cmw 4 2., cmw 4 %s%, cmw 4 x
lamp3=cmw 4 3., cmw 4 %s%, cmw 4 x
by Barito, 2021-2023 (last update apr 2023)
*/

//comment the following line to disable the sync frequency check and block
#define SYNC_MONITOR_ACTIVE

#define OUTPUTS 8

int rID;
int value;

int periodSum;
int periodIst;
int periodAWG;
int sCounter;
int fq;
const int samples = 100;
boolean enableState;
boolean prevEnState;

struct serialRoute {int pin; int pID;} 
serialRoute[OUTPUTS] = {
  //arduino Output pin - serial message ID
  {12, 1},  //DIGITAL OUT 1 is connected to Arduino pin 12. If a message ID "1" (1.) is received, the output state from the emulator will be routed to that pin.
  {11, 2},  //DIGITAL OUT 2
  {10, 3},  //DIGITAL OUT 3
  {9, 4},   //DIGITAL OUT 4
  {2, 5},   //DIGITAL OUT 5
  {3, 6},   //DIGITAL OUT 6
  {4, 7},   //DIGITAL OUT 7
  {5, 8}    //DIGITAL OUT 8
};

const byte hSyncPin = 8;
const byte LED = 13;
const byte vAmpDisable = 7;
const byte VFEpin = A0;
boolean VFEstate;
boolean VFE; //video force enable flag
unsigned long VFEdbTime;

void setup() {
  for (int j = 0; j < OUTPUTS; j++){
    pinMode(serialRoute[j].pin, OUTPUT);
  }
  pinMode(vAmpDisable, OUTPUT); //VIDEO AMP DISABLE
  pinMode(LED, OUTPUT);//BUILT-IN LED
  pinMode(hSyncPin, INPUT_PULLUP); //SYNC MEASURE
  pinMode(VFEpin, INPUT_PULLUP); //VFE toggle switch
  
  VFEstate = digitalRead(VFEpin);
  VFE = 0;//video output not forced
  #ifdef SYNC_MONITOR_ACTIVE
    digitalWrite(vAmpDisable, HIGH); //DISABLE
    digitalWrite(LED, LOW);
  #else
    digitalWrite(vAmpDisable, LOW); //ENABLE
    digitalWrite(LED, HIGH);
  #endif
  //fq = 20; //31KHZ, 32 us (VGA)
  //fq = 29; //25KHZ, 40 us (EGA)
  fq = 55; //15KHZ - default, 66 us (CGA)
  Serial.begin(9600);
}

void loop(){
  DigitalOut();
  #ifdef SYNC_MONITOR_ACTIVE
  VFEset();
  if(VFE == 0){
    FreqCheck();
  }
  else{
    digitalWrite(vAmpDisable, LOW); //ENABLE
    digitalWrite(LED, HIGH);
  }
  #endif
  delay(5);
}

void VFEset(){
  if (millis()-VFEdbTime > 400 /*long enought delay time*/ && digitalRead(VFEpin) !=  VFEstate){
    VFEstate = !VFEstate;
    VFEdbTime = millis();
    if(VFEstate == LOW){
      VFE = !VFE;
    }
  }
}

void DigitalOut(){
  while (Serial.available()>0){
    rID = Serial.parseInt();
    value = Serial.parseInt();
    if (Serial.read() == 'x'){
      for (int j=0; j<OUTPUTS; j++){
        if (rID == serialRoute[j].pID){
          digitalWrite(serialRoute[j].pin, value);
          //Serial.println();
        }
      }
    }
  }
}

void FreqCheck(){
  periodIst = pulseIn(hSyncPin, HIGH);//time (in us) between a high and low pulse (negtive sync, 5% duty cicle). pulseIn holds the code, so this function will alter the whole sketch behaviour if no pulse incomes or a timeout is defined. Timeout brakes the function (don't ask...) so it's not defined. Keep this in mind while debuggin!!
  periodSum = periodSum + periodIst;
  sCounter++;
  if(sCounter > samples){
    periodAWG = (periodSum/sCounter);
    periodSum = 0;
    sCounter = 0;
    if(periodAWG > fq){
      enableState = 1;
    }
    else {
      enableState = 0;
    }
    if (enableState != prevEnState){//take action on state change only
      prevEnState = enableState;
      digitalWrite(vAmpDisable, !enableState);
      digitalWrite(LED, enableState);
    }
  }
}
