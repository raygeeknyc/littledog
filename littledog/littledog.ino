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

#include <SoftwareServo.h>
SoftwareServo frontservo,backservo;
#define left_full 60
#define right_full 120
#define center 90

const byte left_part = (left_full + ((right_full - left_full) / 5));
const byte right_part = (right_full - ((right_full - left_full) / 5));

#define front_pin 9
#define rear_pin 10
#define led_pin 8
#define trig_pin 6
#define echo_pin 7

#define post_step_delay 15
#define pre_step_delay 100
#define max_refresh_delay 25

int led_level;
bool pulsing;
bool led_dir;
#define led_off 0
#define led_min 5
#define led_max 80
#define led_step 4
#define led_step_duration 80
#define shine_duration 4000
unsigned long int led_step_at;
unsigned long int shine_end_at;

#define default_distance 100

// {forward leg's servo's positition, rear leg's servo's position, ...}
char forwards_steps[] =   {
  left_full, left_part, right_full, right_part, left_full, left_part, right_full, right_part};
char backwards_steps[] =  {
  right_part, right_full, left_part, left_full, right_part, right_full, left_part, left_full};
char turn_fwd_steps[] =   {
  left_full, left_part, left_full, right_full, right_full, right_part, left_part, right_part};

int step_seq;
int steps_since_reversal, steps_since_forward, steps_since_turn;

int walking_direction;
#define forwards 0
#define backwards 1
#define turn 2

#define avoidance_threshold_cm 15
#define backwards_limit 8
#define turn_limit 8

int get_distance() {
  digitalWrite(trig_pin, LOW); 
  delayMicroseconds(2); 

  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10); 

  digitalWrite(trig_pin, LOW);

  int duration = pulseIn(echo_pin, HIGH);

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
  led_level = led_max;
  led_dir = false;
  led_step_at = 0;
  analogWrite(led_pin, led_level);
  pulsing = true;
}

void stop_led_pulsing() {
#ifdef _DEBUG
  Serial.println("LED stop pulsing ");
#endif
  led_level = led_off;
  analogWrite(led_pin, led_level);
  pulsing = false;
}

void startLedShining() {
  stop_led_pulsing();
  shine_end_at = millis() + shine_duration;
  analogWrite(led_pin, led_max);
#ifdef _DEBUG
  Serial.println("LED shining ");
#endif
}

void stopLedShining() {
#ifdef _DEBUG
  Serial.println("LED stop shining ");
#endif
  shine_end_at = 0;
  analogWrite(led_pin, led_max);
}

void expire_led() {
  if (!pulsing) {
    return;
  }
  if (!is_shining() && shine_end_at != 0) {
    stopLedShining();
    return;
  }
  if (millis() <= led_step_at) {
    return;
  }
  led_step_at = millis() + led_step_duration;

  if (led_dir) {
    if (led_level < led_max) {
      led_level+=led_step;
    } 
    else { 
      led_dir = false;
    }
  } 
  else {
    if (led_level > led_min) {
      led_level-=led_step;
    } 
    else { 
      led_dir = true;
    }
  }
  analogWrite(led_pin, led_level);
}

boolean is_shining() {
  return (shine_end_at != 0) && (shine_end_at > millis());
}

void setup() {
#ifdef _DEBUG
  Serial.begin(9600);
  Serial.println("setup");
#endif 
  pinMode(led_pin, OUTPUT);
  for (int i=0; i<4; i++) {
    digitalWrite(led_pin, HIGH);
    delay(500);
    digitalWrite(led_pin, LOW);
    delay(200);
  }    
  walking_direction = forwards;
  step_seq = 0;
  steps_since_reversal = 0;
  steps_since_forward = 0;
  steps_since_turn = 0;
  frontservo.attach(front_pin);
  backservo.attach(rear_pin);
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);
  shine_end_at = 0;
  start_led_pulsing();
#ifdef _DEBUG
  Serial.println("/setup");
#endif 
}

void loop() {
  char *current_gait;
  // We need to refresh software servos every 50ms or more so we cannot just have an aribitrarily long delay here
  for (int i=0; i<pre_step_delay; i+= (pre_step_delay / max_refresh_delay)) {
    expire_led();
    delay(max_refresh_delay);
    frontservo.refresh();
    backservo.refresh();
  }
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
  if (walking_direction == forwards && get_distance() <= avoidance_threshold_cm) {
#ifdef _DEBUG
    Serial.println("backing up");
#endif
    walking_direction = backwards;
    steps_since_reversal = 0;
    step_seq = 1;
    stop_led_pulsing();
  }

  // If we're going backwards, limit how long we back up and then blink the LED
  if (walking_direction == backwards && steps_since_reversal > backwards_limit) {
#ifdef _DEBUG
    Serial.println("Done backing up");
#endif
    if (get_distance() >= avoidance_threshold_cm) {
#ifdef _DEBUG
      Serial.println("Turning");
#endif
      walking_direction = turn;
      steps_since_turn = 0;
      step_seq = 1;
      startLedShining();
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
