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
const uint8_t LOOP_DELAY_MS = 5;
const int8_t MOVE_POWER = 50;

char current_command = 'S';
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;

uint16_t power_to_useconds(int8_t val) {
  return map(val, -100, 100, MOTORS_MIN, MOTORS_MAX);
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

void read_python_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (
      command == 'S'
      || command == 'F'
      || command == 'B'
      || command == 'L'
      || command == 'R'
      || command == 'D'
    ) {
      current_command = command;
      last_command_ms = millis();
    }
  }
}

void stop_if_command_lost() {
  if (current_command != 'S' && millis() - last_command_ms > COMMAND_TIMEOUT_MS) {
    current_command = 'S';
  }
}

void apply_current_command() {
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
      set_motors(0, 0, 0, 0);
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

  for (int i = 0; i < 8; i++) {
    set_motors(0, 0, 0, 0);
    delay(500);
    digitalWrite(LED_BUILTIN, i % 2 == 0 ? HIGH : LOW);
  }

  digitalWrite(LED_BUILTIN, LOW);
  last_command_ms = millis();
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  apply_current_command();
  send_emg_sample();
  delay(LOOP_DELAY_MS);
}
