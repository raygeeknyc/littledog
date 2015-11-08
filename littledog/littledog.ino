/*
 * @author("Raymond Blum" <raymond@insanegiantrobots.com>)
 * Targeted for an adafruit pro trinket but should work on any Arduino compatible controller
 *
 *******************
 Littledog by Raymond Blum is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License.
 *******************
 see http://www.insanegiantrobots.com/littledog
 */

#define _NO_DEBUG_
#include <Servo.h>

// Adjust these to center your servos
#define LEG_FRONT_CENTER 85
#define LEG_REAR_CENTER 90
// Adjust this to define how far a full step swings the leg
#define GAIT_RANGE 70

const int LEGS_FRONT_LEFT_FULL = LEG_FRONT_CENTER - (GAIT_RANGE / 2);
const int LEGS_FRONT_RIGHT_FULL = LEG_FRONT_CENTER + (GAIT_RANGE / 2);

const int LEGS_REAR_LEFT_FULL = LEG_REAR_CENTER - (GAIT_RANGE / 2);
const int LEGS_REAR_RIGHT_FULL = LEG_REAR_CENTER + (GAIT_RANGE / 2);

const byte LEGS_FRONT_LEFT_PART = LEG_FRONT_CENTER - (GAIT_RANGE / 5);
const byte LEGS_FRONT_RIGHT_PART = LEG_FRONT_CENTER + (GAIT_RANGE / 5);

const byte LEGS_REAR_LEFT_PART = LEG_REAR_CENTER - (GAIT_RANGE / 5);
const byte LEGS_REAR_RIGHT_PART = LEG_REAR_CENTER + (GAIT_RANGE / 5);

Servo frontservo, backservo;

// Set this to control how long we raom after activity before going to sleep
#define WAKE_DURATION 20000
#define SLEEP_INTERVAL 100

#define SNORE_DURATION 200
#define SNORE_TONE 100

#define CHIRP_DURATION 200
#define CHIRP_TONE 450

#define BEEP_DURATION 300
#define BEEP_TONE 300

#define PING_SAMPLES 5
#define LIGHT_SAMPLES 5
#define PING_MAX_DISTANCE 100

// Define which pins each of our sensors and actuators are connected to
#define PIN_SERVO_FRONT 9
#define PIN_SERVO_REAR 10
#define PIN_LED 6
#define PIN_PING_TRIGGER 11
#define PIN_PING_ECHO 12
#define PIN_CDS A3
#define PIN_SPEAKER 4

#define DELAY_POST_STEP_MS 15
#define DELAY_PRE_STEP_MS 700

int sleeping_, led_shining_;

unsigned long int awake_until_;
unsigned long int led_shine_until_;


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

#define default_distance 100
int distance_, prev_distance_;

// {forward leg's servo's position, rear leg's servo's position, ...}
char no_steps[] = {};
char forwards_steps[] =   {
  LEGS_FRONT_LEFT_FULL, LEGS_REAR_LEFT_PART, LEGS_FRONT_RIGHT_FULL, LEGS_REAR_RIGHT_PART, LEGS_FRONT_LEFT_FULL, LEGS_REAR_LEFT_PART, LEGS_FRONT_RIGHT_FULL, LEGS_REAR_RIGHT_PART
};
char backwards_steps[] =  {
  LEGS_FRONT_RIGHT_PART, LEGS_REAR_RIGHT_FULL, LEGS_FRONT_LEFT_PART, LEGS_REAR_LEFT_FULL, LEGS_FRONT_RIGHT_PART, LEGS_REAR_RIGHT_FULL, LEGS_FRONT_LEFT_PART, LEGS_REAR_LEFT_FULL
};
char turn_fwd_steps[] =   {
  LEGS_FRONT_LEFT_FULL, LEGS_REAR_LEFT_PART, LEGS_FRONT_LEFT_FULL, LEGS_REAR_RIGHT_FULL, LEGS_FRONT_RIGHT_FULL, LEGS_REAR_RIGHT_PART, LEGS_FRONT_LEFT_PART, LEGS_REAR_RIGHT_PART
};
char *current_gait = no_steps;

int step_seq;
int steps_since_reversal, steps_since_forward, steps_since_turn;

int walking_direction_;
#define FORWARDS 0
#define BACKWARDS 1
#define TURNING 2

#define THRESHOLD_LIGHT_CHANGE 200

int light_, prev_light_;

#define THRESHOLD_DISTANCE_CHANGED 40
#define THRESHOLD_AVOIDANCE_CM  15
#define LIMIT_BACKWARDS 8
#define LIMIT_TURNING 8

void setSensorBaseLevels() {
  prev_light_ = light_ = getLightLevel();
  prev_distance_ = distance_ = getDistance();
}

void snore() {
  noTone(PIN_SPEAKER);
  tone(PIN_SPEAKER, SNORE_TONE, SNORE_DURATION);
}

void sleepUntilWoken() {
  setSensorBaseLevels();
  snore();
  startLedPulsing();
  sleeping_ = true;
  while (sleeping_) {
    delay(SLEEP_INTERVAL);
    periodicRefresh();
    if (activityDetected()) {
      wakeUp();
    }
  }
}

void setGoToSleepTime() {
  awake_until_ = millis() + WAKE_DURATION;
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
  blinkLed();
}

void refreshSensors() {
  prev_light_ = light_;
  light_ = getLightLevel();
  light_asof_ = millis();

  prev_distance_ = distance_;
  distance_ = getDistance();
  distance_asof_ = millis();
}

int getLightLevel() {
  int sum = 0, min = 9999, max = -1;
  for (int i = 0; i < LIGHT_SAMPLES; i++) {
    int sample = analogRead(PIN_CDS);
    if (sample < min) min = sample;
    if (sample > max) max = sample;
    sum += sample;
  }
  sum -= (min + max);
  return (sum / (LIGHT_SAMPLES - 2));
}

int getDistance() {
  int sum = 0, min = 9999, max = -1;
  for (int i = 0; i < PING_SAMPLES; i++) {
    int sample = getPing();
    if (sample < min) min = sample;
    if (sample > max) max = sample;
    sum += sample;
  }
  sum -= (min + max);
  return (sum / (PING_SAMPLES - 2));
}

boolean isLightLevelChanged() {
  boolean changed = abs(light_ - prev_light_) > THRESHOLD_LIGHT_CHANGE;
  #ifdef DEBUG_
  Serial.print("light: ");
  Serial.print(prev_light_);
  Serial.print(" --> ");
  Serial.print(light_);
  Serial.print(" ");
  Serial.print(abs(light_ - prev_light_));
  Serial.print(" == ");
  Serial.println(changed ? "***CHANGE***" : "");
  #endif
  return changed;
}

boolean isDistanceChanged() {
  return (abs(distance_ - prev_distance_) > THRESHOLD_DISTANCE_CHANGED);
}

boolean  activityDetected() {
  if (isLightLevelChanged()) {
    return true;
  }
  if (isDistanceChanged()) {
    return true;
  }
  return false;
}

int getPing() {
  digitalWrite(PIN_PING_TRIGGER, LOW);
  delayMicroseconds(2); 
  digitalWrite(PIN_PING_TRIGGER, HIGH);
  delayMicroseconds(10); 

  digitalWrite(PIN_PING_TRIGGER, LOW);

  int duration = pulseIn(PIN_PING_ECHO, HIGH);

  if (duration <= 0) {
     return 0;
  }
  //Calculate the distance (in cm) based on the speed of sound.
  float HR_dist = duration/58.2;
  #ifdef DEBUG_
  Serial.print("ping: ");
  Serial.println(HR_dist);
  #endif
  return min(HR_dist,PING_MAX_DISTANCE);
}

void startLedPulsing() {
  #ifdef DEBUG_
  Serial.println("LED pulsing ");
  #endif
  led_level_ = LED_MIN;
  led_dir_ = 1;
  led_step_at = 0;
  analogWrite(PIN_LED, led_level_);
  led_pulsing_ = true;
  led_shining_ = false;
}

void stopLedPulsing() {
  #ifdef DEBUG_
  Serial.println("LED stop pulsing ");
  #endif
  led_level_ = LED_OFF;
  analogWrite(PIN_LED, led_level_);
  led_pulsing_ = false;
}

void beep() {
  noTone(PIN_SPEAKER);
  tone(PIN_SPEAKER, BEEP_TONE, BEEP_DURATION);
  #ifdef DEBUG_
  Serial.println("beep");
  #endif
}


void chirp() {
  noTone(PIN_SPEAKER);
  tone(PIN_SPEAKER, CHIRP_TONE, CHIRP_DURATION);
  #ifdef DEBUG_
  Serial.println("beep");
  #endif
}

void blinkLed() {
  stopLedPulsing();
  analogWrite(PIN_LED, LED_MAX);
  #ifdef DEBUG_
  Serial.println("blink");
  #endif
  led_shining_ = 1;
  led_shine_until_ = millis() + LED_BLINK_DURATION;
}

void stopLedShining() {
  #ifdef DEBUG_
  Serial.println("LED stop shining ");
  #endif
  led_shine_until_ = 0;
  analogWrite(PIN_LED, LED_OFF);
  led_shining_ = 0;
}

void expireLed() {
  if (led_shining_) {
    if (led_shine_until_ > 0 && led_shine_until_ <  millis()) {
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
      led_level_ += LED_STEP;
    }
    else {
      led_dir_ = false;
    }
  }
  else {
    if (led_level_ > LED_MIN) {
      led_level_ -= LED_STEP;
    }
    else {
      led_dir_ = true;
    }
  }
  analogWrite(PIN_LED, led_level_);
}

void setup() {
  #ifdef DEBUG_
  Serial.begin(115200);
  Serial.println("setup");
  #endif
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PING_TRIGGER, OUTPUT);
  pinMode(PIN_SPEAKER, OUTPUT);
  pinMode(PIN_PING_ECHO, INPUT);
  pinMode(PIN_CDS, INPUT);
  chirp();
  for (int i = 0; i < 4; i++) {
    analogWrite(PIN_LED, 120);
    delay(500);
    analogWrite(PIN_LED, 60);
    delay(200);
  }
  frontservo.attach(PIN_SERVO_FRONT);
  backservo.attach(PIN_SERVO_REAR);
  frontservo.write(LEG_FRONT_CENTER);
  backservo.write(LEG_REAR_CENTER);

  walking_direction_ = FORWARDS;
  step_seq = 0;
  steps_since_reversal = 0;
  steps_since_forward = 0;
  steps_since_turn = 0;
  led_shining_ = false;
  startLedPulsing();
  #ifdef DEBUG_
  Serial.println("/setup");
  #endif
  setGoToSleepTime();
}
void loop() {  
  if (isTimeToSleep()) {
    sleepUntilWoken();
    setGoToSleepTime();
  } else {
    roamUntilSleep();
  }
}

void roamUntilSleep() {
  stopLedPulsing();
  stopLedShining();
  while (!isTimeToSleep()) {
    periodicRefresh();
    takeNextStep();
  }
}

void takeNextStep() {
  delay(DELAY_PRE_STEP_MS);
  #ifdef DEBUG_
  Serial.print("walking direction ");
  Serial.println(walking_direction_);
  #endif

  // Handle walking while in a sequence
  switch (walking_direction_) {
    case FORWARDS:
      #ifdef DEBUG_
      Serial.println("walking fwd");
      #endif
      stopLedPulsing();
      stopLedShining();
      current_gait = forwards_steps;
      break;
    case BACKWARDS:
      #ifdef DEBUG_
      Serial.println("walking bwd");
      #endif
      current_gait = backwards_steps;
      break;
    case TURNING:
      #ifdef DEBUG_
      Serial.println("turning");
      #endif
      current_gait = turn_fwd_steps;
      break;
  }
  frontservo.write(current_gait[2 * step_seq]);
  backservo.write(current_gait[(2 * step_seq) + 1]);

  delay(DELAY_POST_STEP_MS);

  step_seq += 1;
  steps_since_reversal += 1;
  steps_since_forward += 1;
  steps_since_turn += 1;

  // There are only 4 steps in a sequence
  if (step_seq > 3) {
    step_seq = 0;
  }

  // If there's proximity and we're going forward, start backing up, turn off the LED
  if (walking_direction_ == FORWARDS && getDistance() <= THRESHOLD_AVOIDANCE_CM) {
    #ifdef DEBUG_
    Serial.println("backing up");
    #endif
    walking_direction_ = BACKWARDS;
    steps_since_reversal = 0;
    step_seq = 0;
    blinkLed();
    //beep();
  }

  // If we're going backwards, limit how long we back up and then blink the LED
  if (walking_direction_ == BACKWARDS && steps_since_reversal > LIMIT_BACKWARDS) {
    #ifdef DEBUG_
    Serial.println("Done backing up");
    #endif
    if (getDistance() >= THRESHOLD_AVOIDANCE_CM) {
      #ifdef DEBUG_
      Serial.println("Turning");
      #endif
      walking_direction_ = TURNING;
      steps_since_turn = 0;
      step_seq = 0;
    } else {
      #ifdef DEBUG_
      Serial.println("All clear, going fwd");
      #endif
      walking_direction_ = FORWARDS;
      steps_since_forward = 0;
      step_seq = 0;
    }
  }
  // If we're turning, limit how long we turn and then go forwards with the LED pulsing
  if ((walking_direction_ == TURNING)  && steps_since_turn > LIMIT_TURNING) {
    #ifdef DEBUG_
    Serial.println("Done turning");
    #endif
    walking_direction_ = FORWARDS;
    steps_since_forward = 0;
    step_seq = 0;
  }
}
