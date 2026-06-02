#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t MOTORS_MIN = 1100;
const uint16_t MOTORS_MAX = 1900;

const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;
const uint16_t COMMAND_TIMEOUT_MS = 1000;
const uint16_t STOP_LOCK_MS = 0;
const uint16_t STOP_HOLD_BURST_PERIOD_MS = 500;
const uint8_t STOP_BURST_COUNT = 6;
const uint8_t STOP_BURST_DELAY_MS = 20;

const int8_t MOVE_POWER = 45;

char current_command = 'S';
bool is_armed = false;
bool msp_started = false;
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;
uint32_t stop_lock_until_ms = 0;
uint32_t last_stop_burst_ms = 0;
int8_t target_m1 = 0;
int8_t target_m2 = 0;
int8_t target_m3 = 0;
int8_t target_m4 = 0;
int8_t sent_m1 = 0;
int8_t sent_m2 = 0;
int8_t sent_m3 = 0;
int8_t sent_m4 = 0;
bool force_motor_update = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("EMG_ROV_CONTROL_READY");
  Serial.println("DISARMED");

  pinMode(EMG_PIN, INPUT);
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  set_motor_targets(0, 0, 0, 0);

  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
  }
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  hold_stop_window();
  update_motors();
  send_emg_sample();
}

void read_python_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    apply_command(command);
  }
}

void apply_command(char command) {
  last_command_ms = millis();

  if (command == 'D') {
    apply_stop(true);
    return;
  }

  if (command == 'A') {
    return;
  }

  if (is_stop_locked() && is_move_command(command)) {
    return;
  }

  if (command == current_command && !force_motor_update) {
    return;
  }

  switch (command) {
    case 'S':
      apply_stop(false);
      break;
    case 'F':
      start_msp_if_needed();
      is_armed = true;
      current_command = 'F';
      digitalWrite(LED_BUILTIN, HIGH);
      set_motor_targets(MOVE_POWER, 0, MOVE_POWER, 0);
      force_motor_update = true;
      break;
    case 'B':
      start_msp_if_needed();
      is_armed = true;
      current_command = 'B';
      digitalWrite(LED_BUILTIN, HIGH);
      set_motor_targets(-MOVE_POWER, 0, -MOVE_POWER, 0);
      force_motor_update = true;
      break;
    case 'L':
      start_msp_if_needed();
      is_armed = true;
      current_command = 'L';
      digitalWrite(LED_BUILTIN, HIGH);
      set_motor_targets(MOVE_POWER, 0, -MOVE_POWER, 0);
      force_motor_update = true;
      break;
    case 'R':
      start_msp_if_needed();
      is_armed = true;
      current_command = 'R';
      digitalWrite(LED_BUILTIN, HIGH);
      set_motor_targets(-MOVE_POWER, 0, MOVE_POWER, 0);
      force_motor_update = true;
      break;
    default:
      apply_stop(false);
      break;
  }
}

void stop_if_command_lost() {
  if (is_armed && millis() - last_command_ms > COMMAND_TIMEOUT_MS) {
    apply_stop(true);
  }
}

void send_emg_sample() {
  uint32_t now = millis();
  if (now - last_sample_ms < SAMPLE_PERIOD_MS) {
    return;
  }

  last_sample_ms = now;
  Serial.println(analogRead(EMG_PIN));
}

void stop_motors() {
  apply_stop(true);
}

void apply_stop(bool disarm) {
  digitalWrite(LED_BUILTIN, LOW);
  stop_lock_until_ms = millis() + STOP_LOCK_MS;
  set_motor_targets(0, 0, 0, 0);

  if (msp_started && (current_command != 'S' || is_armed || force_motor_update)) {
    send_stop_burst();
  }

  current_command = 'S';
  force_motor_update = false;

  if (disarm) {
    is_armed = false;
  }
}

bool is_stop_locked() {
  return (int32_t)(stop_lock_until_ms - millis()) > 0;
}

bool is_move_command(char command) {
  return command == 'F' || command == 'B' || command == 'L' || command == 'R';
}

void hold_stop_window() {
  if (!is_stop_locked() || !msp_started) {
    return;
  }

  if (millis() - last_stop_burst_ms >= STOP_HOLD_BURST_PERIOD_MS) {
    send_stop_burst();
  }
}

void set_motor_targets(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {
  target_m1 = m1;
  target_m2 = m2;
  target_m3 = m3;
  target_m4 = m4;
}

void update_motors() {
  bool target_changed = (
    target_m1 != sent_m1
    || target_m2 != sent_m2
    || target_m3 != sent_m3
    || target_m4 != sent_m4
  );

  if (is_stop_locked() || !is_armed || (!force_motor_update && !target_changed)) {
    return;
  }

  start_msp_if_needed();
  send_motor_values(target_m1, target_m2, target_m3, target_m4);
  force_motor_update = false;
}

void start_msp_if_needed() {
  if (msp_started) {
    return;
  }

  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);
  msp_started = true;
}

uint16_t power_to_useconds(int8_t power) {
  power = constrain(power, -100, 100);
  return map(power, -100, 100, MOTORS_MIN, MOTORS_MAX);
}

void send_motor_values(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {
  start_msp_if_needed();

  uint16_t data[8] = {
    power_to_useconds(m1),
    power_to_useconds(m2),
    power_to_useconds(m3),
    power_to_useconds(m4),
    power_to_useconds(0),
    power_to_useconds(0),
    power_to_useconds(0),
    power_to_useconds(0)
  };

  msp.send(MSP_SET_MOTOR, data, 16);

  sent_m1 = m1;
  sent_m2 = m2;
  sent_m3 = m3;
  sent_m4 = m4;
}

void set_motors(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {
  send_motor_values(m1, m2, m3, m4);
}

void send_stop_burst() {
  for (uint8_t i = 0; i < STOP_BURST_COUNT; i++) {
    send_motor_values(0, 0, 0, 0);
    delay(STOP_BURST_DELAY_MS);
  }

  last_stop_burst_ms = millis();
}
