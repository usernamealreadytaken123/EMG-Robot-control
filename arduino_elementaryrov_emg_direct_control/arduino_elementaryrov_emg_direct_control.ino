#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t MOTORS_MIN = 1100;
const uint16_t MOTORS_MAX = 1900;
const uint16_t DEFAULT_MOTORS_STOP_US = 1500;
const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;
const uint8_t MOVE_POWER = 20;

char current_command = 'S';
uint32_t last_sample_ms = 0;
uint16_t motors_stop_us = DEFAULT_MOTORS_STOP_US;

uint16_t stop_value_from_digit(char command) {
  switch (command) {
    case '0':
      return 1000;
    case '1':
      return 1050;
    case '2':
      return 1100;
    case '3':
      return 1150;
    case '4':
      return 1200;
    case '5':
      return 1300;
    case '6':
      return 1400;
    case '7':
      return 1450;
    case '8':
      return 1500;
    case '9':
      return 1550;
    default:
      return motors_stop_us;
  }
}

uint16_t power_to_useconds(int8_t power) {
  power = constrain(power, -100, 100);
  return map(power, -100, 100, MOTORS_MIN, MOTORS_MAX);
}

void set_motors(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {
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
}

void stop_motors() {
  uint16_t data[8] = {
    motors_stop_us,
    motors_stop_us,
    motors_stop_us,
    motors_stop_us,
    motors_stop_us,
    motors_stop_us,
    motors_stop_us,
    motors_stop_us
  };

  msp.send(MSP_SET_MOTOR, data, 16);
}

void read_python_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (command >= '0' && command <= '9') {
      motors_stop_us = stop_value_from_digit(command);
      current_command = 'S';
      continue;
    }

    if (command == 'S' || command == 'F' || command == 'B' || command == 'L' || command == 'R' || command == 'D') {
      current_command = command;
    }
  }
}

void send_current_motor_command() {
  switch (current_command) {
    case 'F':
      set_motors(MOVE_POWER, 0, MOVE_POWER, 0);
      break;
    case 'B':
      set_motors(-MOVE_POWER, 0, -MOVE_POWER, 0);
      break;
    case 'L':
      set_motors(MOVE_POWER, 0, -MOVE_POWER, 0);
      break;
    case 'R':
      set_motors(-MOVE_POWER, 0, MOVE_POWER, 0);
      break;
    case 'S':
    case 'D':
    default:
      stop_motors();
      break;
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

void setup() {
  Serial.begin(SERIAL_BAUD);
  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);

  pinMode(EMG_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  for (uint8_t i = 0; i < 8; i++) {
    stop_motors();
    digitalWrite(LED_BUILTIN, i % 2 == 0 ? HIGH : LOW);
    delay(500);
  }

  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  read_python_command();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  send_current_motor_command();
  send_emg_sample();
  delay(5);
}
