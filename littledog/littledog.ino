/*
 * @author("Raymond Blum" <raymond@insanegiantrobots.com>)
 * targeted for an ATTiny44u but should work on any Arduino compatible controller
 *
 *******************
 Littledog by Raymond Blum is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License.
 *******************
 see http://www.insanegiantrobots.com/littledog
 */

//#define _DEBUG
#include <Servo.h>

Servo frontservo,backservo;
const int gait_range=70;
const int center=90;
const int left_full=center-(gait_range/2);
const int right_full=center+(gait_range/2);

const byte left_part = center-(gait_range/5);
const byte right_part = center+(gait_range/5);

#define SLEEP_INTERVAL 600
#define WAKE_DUR 10000

#define BEEP_DURATION 700
#define BEEP_TONE 350
#define PIN_SPEAKER 22

#define PING_SAMPLES 5
#define LIGHT_SAMPLES 5

#define PIN_SERVO_FRONT 9
#define PIN_SERVO_REAR 10
#define PIN_LED 8
#define PIN_PING_TRIGGER 6
#define PIN_PING_ECHO 7
#define PIN_LIGHT 23

#define post_step_delay 15
#define pre_step_delay 30
#define max_refresh_delay 25

int sleeping_, shining_;

unsigned long int awake_until_;
unsigned long int shine_until_;


unsigned long int light_asof_, distance_asof_;

int led_level_;
bool led_pulsing_;
bool led_dir_;
#define LED_OFF 0
#define LED_MIN 5
#define LED_MAX 80
#define LED_STEP 4
#define LED_STEP_DURATION 80
#define LED_BLINK_DURATION 4000
unsigned long int led_step_at;
unsigned long int shine_end_at;

#define default_distance 100
int distance_, prev_distance_;

// {forward leg's servo's positition, rear leg's servo's position, ...}
char no_steps[] = {};
char forwards_steps[] =   {
  left_full, left_part, right_full, right_part, left_full, left_part, right_full, right_part};
char backwards_steps[] =  {
  right_part, right_full, left_part, left_full, right_part, right_full, left_part, left_full};
char turn_fwd_steps[] =   {
  left_full, left_part, left_full, right_full, right_full, right_part, left_part, right_part};
char *current_gait = no_steps;

int step_seq;
int steps_since_reversal, steps_since_forward, steps_since_turn;

int walking_direction;
#define forwards 0
#define backwards 1
#define turn 2

#define THRESHOLD_LIGHT_CHANGE 30
int light_, prev_light_;

#define THRESHOLD_DISTANCE_CHANGED 3
#define avoidance_threshold_cm 15
#define backwards_limit 8
#define turn_limit 8

void setSensorBaseLevels() {
  prev_light_ = getLightLevel();
  prev_distance_ = getDistance();
}

void sleepUntilWoken() {
  setSensorBaseLevels();
  while (sleeping_) {
    delay(SLEEP_INTERVAL);
    periodicRefresh();
    if (activityDetected()) {
      wakeUp();
    }
  }
  awake_until_ = millis()+WAKE_DUR;
}

boolean isTimeToSleep() {
  return (awake_until_ != 0 && awake_until_ < millis());
}

void periodicRefresh() {
    refreshSensors();
    expireLed();
}

void wakeUp() {
  sleeping_ = false;
  chirp();
  blink();
}

void refreshSensors() {
  prev_light_ = light_;
  prev_distance_ = distance_;
  light_ = getLightLevel();
  light_asof_ = millis();
  distance_ = getDistance();
  distance_asof_ = millis();
}

int getLightLevel() {
  int sum = 0, min = 9999, max = -1;
  for (int i=0; i< LIGHT_SAMPLES; i++) {
    int sample = analogRead(PIN_LIGHT);
    if (sample<min) min=sample;
    if (sample>max) max=sample;
    sum += sample;
  }
  sum -= (min + max);
  return (sum/3);
}

int getDistance() {
  int sum = 0, min = 9999, max = -1;
  for (int i=0; i< PING_SAMPLES; i++) {
    int sample = getPing();
    if (sample<min) min=sample;
    if (sample>max) max=sample;
    sum += sample;
  }
  sum -= (min + max);
  return (sum/3);
}

boolean  activityDetected() {
  if (abs(light_ - prev_light_) > THRESHOLD_LIGHT_CHANGE) {
    return true;
  }
  if (abs(distance_ - prev_distance_) > THRESHOLD_DISTANCE_CHANGED) {
    return true;
  }
  return false;
}


/// here from notes

int getPing() {
  digitalWrite(PIN_PING_TRIGGER, LOW); 
  delayMicroseconds(2); 

  digitalWrite(PIN_PING_TRIGGER, HIGH);
  delayMicroseconds(10); 

  digitalWrite(PIN_PING_TRIGGER, LOW);

  int duration = pulseIn(PIN_PING_ECHO, HIGH);

  if (duration <= 0) {
    #ifdef _DEBUG
    Serial.println("default distance ");
    #endif
    return default_distance;
  }
  //Calculate the distance (in cm) based on the speed of sound.
  float HR_dist = duration/58.2;
  #ifdef _DEBUG
  Serial.print("distance ");
  Serial.println(HR_dist);
  #endif
  return int(HR_dist);
}

void start_led_pulsing() {
  #ifdef _DEBUG
  Serial.println("LED pulsing ");
  #endif
  led_level_ = LED_MAX;
  led_dir_ = false;
  led_step_at = 0;
  analogWrite(PIN_LED, led_level_);
  led_pulsing_ = true;
}

void stopLedPulsing() {
  #ifdef _DEBUG
  Serial.println("LED stop pulsing ");
  #endif
  led_level_ = LED_OFF;
  analogWrite(PIN_LED, led_level_);
  led_pulsing_ = false;
}

void chirp() {
  tone(PIN_SPEAKER, BEEP_TONE, BEEP_DURATION);
  #ifdef _DEBUG
  Serial.println("beep");
  #endif
}

void blink() {
  stopLedPulsing();
  analogWrite(PIN_LED, LED_MAX);
  #ifdef _DEBUG
  Serial.println("blink");
  #endif
  shining_ = 1;
}

void stopLedShining() {
  #ifdef _DEBUG
  Serial.println("LED stop shining ");
  #endif
  shine_until_ = 0;
  analogWrite(PIN_LED, LED_MAX);
  shining_ = 0;
}

void expireLed() {
  if (shining_) {
    if (shine_until_ > 0 && shine_until_ <  millis()) {
      stopLedShining();
    }
    return;
  }
  
  if (!led_pulsing_) {
    return;
  }
  
  if (millis() <= led_step_at) {
    return;
  }
  led_step_at = millis() + LED_STEP_DURATION;

  if (led_dir_) {
    if (led_level_ < LED_MAX) {
      led_level_+=LED_STEP;
    } 
    else { 
      led_dir_ = false;
    }
  } 
  else {
    if (led_level_ > LED_MIN) {
      led_level_-=LED_STEP;
    } 
    else { 
      led_dir_ = true;
    }
  }
  analogWrite(PIN_LED, led_level_);
}

boolean is_shining() {
  return (shine_end_at != 0) && (shine_end_at > millis());
}

void setup() {
  #ifdef _DEBUG
  Serial.begin(9600);
  Serial.println("setup");
  #endif 
  frontservo.attach(PIN_SERVO_FRONT);
  backservo.attach(PIN_SERVO_REAR);
  frontservo.write(center);
  backservo.write(center);

  pinMode(PIN_LED, OUTPUT);
  for (int i=0; i<4; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(500);
    digitalWrite(PIN_LED, LOW);
    delay(200);
  }    
  walking_direction = forwards;
  step_seq = 0;
  steps_since_reversal = 0;
  steps_since_forward = 0;
  steps_since_turn = 0;
  pinMode(PIN_PING_TRIGGER, OUTPUT);
  pinMode(PIN_PING_ECHO, INPUT);
  shine_end_at = 0;
  start_led_pulsing();
  #ifdef _DEBUG
  Serial.println("/setup");
  #endif 
}

void loop() {
  periodicRefresh();
  if (isTimeToSleep()) {
    sleepUntilWoken();
  } else {
    roam();
  }
}

void roam() {
  delay(pre_step_delay);
  #ifdef _DEBUG
  Serial.print("walking direction ");
  Serial.println(walking_direction);
  #endif 

  // Handle walking while in a sequence
  switch (walking_direction) {
  case forwards:
    #ifdef _DEBUG
    Serial.println("walking fwd");
    #endif 
    current_gait = forwards_steps;
    break;
  case backwards:
    #ifdef _DEBUG
    Serial.println("walking bwd");
    #endif 
    current_gait = backwards_steps;
    break;
  case turn:
    #ifdef _DEBUG
    Serial.println("turning");
    #endif 
    current_gait = turn_fwd_steps;
    break;
  }
  frontservo.write(current_gait[2*step_seq]);
  backservo.write(current_gait[(2*step_seq)+1]);

  delay(post_step_delay);

  step_seq += 1;
  steps_since_reversal += 1;
  steps_since_forward += 1;
  steps_since_turn += 1;

  // There are only 4 steps in a sequence
  if (step_seq > 3) {
    step_seq = 0;
  }

  // If there's proximity and we're going forward, start backing up, turn off the LED
  if (walking_direction == forwards && getDistance() <= avoidance_threshold_cm) {
    #ifdef _DEBUG
    Serial.println("backing up");
    #endif
    walking_direction = backwards;
    steps_since_reversal = 0;
    step_seq = 1;
    stopLedPulsing();
  }

  // If we're going backwards, limit how long we back up and then blink the LED
  if (walking_direction == backwards && steps_since_reversal > backwards_limit) {
    #ifdef _DEBUG
    Serial.println("Done backing up");
    #endif
    if (getDistance() >= avoidance_threshold_cm) {
      #ifdef _DEBUG
      Serial.println("Turning");
      #endif
      walking_direction = turn;
      steps_since_turn = 0;
      step_seq = 1;
      blink();
    } else {
      #ifdef _DEBUG
      Serial.println("All clear, going fwd");
      #endif
      walking_direction = forwards;
      steps_since_forward = 0;
      step_seq = 1;
      stopLedShining();
      start_led_pulsing();
    }
  }
  // If we're turning, limit how long we turn and then go forwards with the LED pulsing
  if ((walking_direction == turn)  && steps_since_turn > turn_limit) {
    #ifdef _DEBUG
    Serial.println("Done turning");
    #endif
    walking_direction = forwards;
    steps_since_forward = 0;
    step_seq = 1;
    stopLedShining();
    start_led_pulsing();
  }
}
