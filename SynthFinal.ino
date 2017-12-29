
#include <MozziGuts.h>
#include <Oscil.h> // oscillator 
#include <tables/cos2048_int8.h> // table for Oscils to play
#include <Smooth.h>
#include <AutoMap.h> // maps unpredictable inputs to a range
#include <Sample.h> // Sample template
#include <samples/burroughs1_18649_int8.h> // a converted audio sample included in the Mozzi download

const int PIEZO_PIN = 3;  // set the analog input pin for the piezo 
const int threshold = 80;  // threshold value to decide when the detected signal is a knock or not

const int BUTTON_PIN = 4; // set the digital input pin for the button

// use: Sample <table_size, update_rate> SampleName (wavetable)
Sample <BURROUGHS1_18649_NUM_CELLS, AUDIO_RATE> aSample(BURROUGHS1_18649_DATA);
float recorded_pitch = (float) BURROUGHS1_18649_SAMPLERATE / (float) BURROUGHS1_18649_NUM_CELLS;

char button_state, previous_button_state; // variable for reading the pushbutton status       
char latest_button_change;
boolean triggered = false;
float pitch, pitch_change;

// desired carrier frequency max and min, for AutoMap
const int MIN_CARRIER_FREQ = 22;
const int MAX_CARRIER_FREQ = 440;

// desired intensity max and min, for AutoMap, note they're inverted for reverse dynamics
const int MIN_INTENSITY = 700;
const int MAX_INTENSITY = 10;

// desired mod speed max and min, for AutoMap, note they're inverted for reverse dynamics
const int MIN_MOD_SPEED = 10000;
const int MAX_MOD_SPEED = 1;

// 
const int trigPin = 13;
const int echoPin = 12;
long      duration;
int       distance;

AutoMap kMapCarrierFreq(0,1023,MIN_CARRIER_FREQ,MAX_CARRIER_FREQ);
AutoMap kMapIntensity(0,1023,MIN_INTENSITY,MAX_INTENSITY);
AutoMap kMapModSpeed(0,1023,MIN_MOD_SPEED,MAX_MOD_SPEED);

const int KNOB_PIN = 0; // set the input for the knob to analog pin 0
const int LDR1_PIN=1; // set the analog input for fm_intensity to pin 1
const int LDR2_PIN=2; // set the analog input for mod rate to pin 2

Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aCarrier(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> aModulator(COS2048_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kIntensityMod(COS2048_DATA);

int mod_ratio = 5; // brightness (harmonics)
long fm_intensity; // carries control info from updateControl to updateAudio

// smoothing for intensity to remove clicks on transitions
float smoothness = 0.95f;
Smooth <long> aSmoothIntensity(smoothness);


void setup(){
  //Serial.begin(9600); // for Teensy 3.1, beware printout can cause glitches
  Serial.begin(115200); // set up the Serial output so we can look at the piezo values // set up the Serial output so we can look at the light level
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  startMozzi(); // :))
}

void buttonChangePitch(){

  digitalWrite(trigPin, LOW);
  //delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  distance= duration*0.034/2;

  // read the state of the pushbutton value:
  button_state = digitalRead(BUTTON_PIN);

  // has the button state changed?
  if(button_state != previous_button_state){


    // if the latest change was a press, pitch up, else pitch down
    if (button_state == HIGH) {
      pitch = 2.f * recorded_pitch;
    } 
    else {
      pitch = 0.5f * recorded_pitch;
    }
  }
  previous_button_state = button_state;
}

void updateControl(){
  // read the knob
  int knob_value = mozziAnalogRead(KNOB_PIN); // value is 0-1023

  // read the piezo
  int piezo_value = mozziAnalogRead(PIEZO_PIN); // value is 0-1023
  
  // map the knob to carrier frequency
  int carrier_freq = kMapCarrierFreq(knob_value);
  
  //calculate the modulation frequency to stay in ratio
  int mod_freq = carrier_freq * mod_ratio;
  
  // set the FM oscillator frequencies
  aCarrier.setFreq(carrier_freq); 
  aModulator.setFreq(mod_freq);

  // only trigger once each time the piezo goes over the threshold
  if (piezo_value>threshold) {
    if (!triggered) {
      pitch = recorded_pitch;
      aSample.start();
      triggered = true;
    }
  } else {
    triggered = false;
  }
  buttonChangePitch();
  aSample.setFreq(pitch);

  // read the light dependent resistor on the width Analog input pin
  int LDR1_value= mozziAnalogRead(LDR1_PIN); // value is 0-1023
  // print the value to the Serial monitor for debugging
  Serial.print("LDR1 = "); 
  Serial.print(LDR1_value);
  Serial.print("\t"); // prints a tab

  int LDR1_calibrated = kMapIntensity(LDR1_value);
  Serial.print("LDR1_calibrated = ");
  Serial.print(LDR1_calibrated);
  Serial.print("\t"); // prints a tab
  
  // calculate the fm_intensity
  fm_intensity = ((long)LDR1_calibrated * (kIntensityMod.next()+128))>>8; // shift back to range after 8 bit multiply
  Serial.print("fm_intensity = ");
  Serial.print(fm_intensity);
  Serial.print("\t"); // prints a tab
  
  // read the light dependent resistor on the speed Analog input pin
  int LDR2_value= mozziAnalogRead(LDR2_PIN); // value is 0-1023
  Serial.print("LDR2 = "); 
  Serial.print(LDR2_value);
  Serial.print("\t"); // prints a tab
  
  // use a float here for low frequencies
  float mod_speed = (float)kMapModSpeed(LDR2_value)/1000;
  Serial.print("   mod_speed = ");
  Serial.print(mod_speed);
  Serial.print("\t"); // prints a tab
  kIntensityMod.setFreq(mod_speed);

  // Prints the distance on the Serial Monitor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print("\t"); // prints a tab


  // print the value to the Serial monitor for debugging
  Serial.print("piezo value = ");
  Serial.print(piezo_value);  
  Serial.print("\t"); // prints a tab


  Serial.println(); // finally, print a carraige return for the next line of debugging info
}

int updateAudio(){
  if (triggered == true)
    return aSample.next();
  else{
  long modulation = aSmoothIntensity.next(fm_intensity) * aModulator.next();
  return aCarrier.phMod(modulation);
  }
}


void loop(){
  audioHook();
}
