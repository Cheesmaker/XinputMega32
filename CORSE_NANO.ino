/*
CORSE - PC to JAMMA interface board for generic arcade racing cabinets.
Arduino NANO
This sketch reads outputs signals from Howard Casto's Mamehooker, computes sync frequency 
and disables video amp when frequencies are higher than CGA
Here is an example of mamehooker .ini code:(messages sent to Arduino serial port COM4)

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

//comment the following line if you want a CONTINUOUS sync check. Otherwise sync routine will be deactivated after first sync success
#define SYNC_FOREVER

#define OUTPUTS 8

int rID;
int value;

int periodSum;
int periodIst;
int sCounter;
const int fq = 35; //15KHZ - default, 66 us (CGA)
const int samples = 20;
boolean enableState;
boolean prevEnState;

//digital out 1 is connected to Arduino pin 12. Digital out 2 is connected to Arduino pin 11. Digital out 3 ... you say!
const int out_pin[] = {12, 11, 10, 9, 2, 3, 4, 5};

const byte hSyncPin = 8;
const byte LED = 13;
const byte vAmpDisable = 7;
const byte VFEpin = A0;
boolean VFEstate;
boolean VFE; //video force enable flag
unsigned long VFEdbTime;

void setup() {
  for (int j = 0; j < OUTPUTS; j++){
    pinMode(out_pin[j], OUTPUT);
  }
  pinMode(vAmpDisable, OUTPUT); //VIDEO AMP DISABLE
  pinMode(LED, OUTPUT);//BUILT-IN LED
  pinMode(hSyncPin, INPUT_PULLUP); //SYNC MEASURE
  pinMode(VFEpin, INPUT_PULLUP); //VFE toggle switch
  #ifdef SYNC_MONITOR_ACTIVE
    enableState = 0;
  #else
    enableState = 1;
  #endif
  digitalWrite(vAmpDisable, !enableState);
  digitalWrite(LED, enableState);
  VFEstate = digitalRead(VFEpin);
  VFE = 0;//video output not forced to enable
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
  delay(10);
}

void VFEset(){
  if (millis()-VFEdbTime > 400 /*long enought delay time*/ && digitalRead(VFEpin) !=  VFEstate){
    VFEstate = !VFEstate;
    VFEdbTime = millis();
    if(VFEstate == LOW){//toggle Video Force Enable flag
      VFE = !VFE;
    }
  }
}

void DigitalOut(){
  while (Serial.available()>0){
    rID = Serial.parseInt();    //first byte (progessive output number)
    value = Serial.parseInt();  //second byte (output state)
    if (Serial.read() == 'x'){  //third byte (end of message)
      if(rID >0 && rID <=OUTPUTS){
        digitalWrite(out_pin[rID-1], value);
      }
    }
  }
}

void FreqCheck(){
  periodIst = pulseIn(hSyncPin, HIGH);//time (in us) between a high and low pulse (negtive sync, 5% duty cicle). "pulseIn" function holds the code, so this function will stop the execution of other functions if no pulse incomes or a timeout is defined. Timeout brakes the function (don't ask...) so it's not defined. Keep this in mind while debuggin!!
  periodSum = periodSum + periodIst;
  sCounter++;
  if(sCounter >= samples){
    if(periodSum > fq * samples){
      enableState = 1;
    }
    #ifdef SYNC_FOREVER
      else {
        enableState = 0;
      }
    #endif
    periodSum = 0;
    sCounter = 0;
    if (enableState != prevEnState){//take action on state change only
      prevEnState = enableState;
      digitalWrite(vAmpDisable, !enableState);
      digitalWrite(LED, enableState);
    }
  }
}
