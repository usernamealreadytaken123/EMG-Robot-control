#include <Arduino.h>
#include "pwm.h"

SOFTPWM_DEFINE_CHANNEL(8, DDRB, PORTB, PORTB0);
SOFTPWM_DEFINE_CHANNEL(9, DDRB, PORTB, PORTB1);
SOFTPWM_DEFINE_CHANNEL(10, DDRB, PORTB, PORTB2);
SOFTPWM_DEFINE_CHANNEL(11, DDRB, PORTB, PORTB3);
SOFTPWM_DEFINE_CHANNEL(12, DDRB, PORTB, PORTB4);
SOFTPWM_DEFINE_CHANNEL(13, DDRB, PORTB, PORTB5);

SOFTPWM_DEFINE_OBJECT_WITH_PWM_LEVELS(20, 101);
SOFTPWM_DEFINE_EXTERN_OBJECT_WITH_PWM_LEVELS(20, 101);

const uint32_t SERIAL_BAUD = 115200;
const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;
const uint16_t COMMAND_TIMEOUT_MS = 1000;
const int8_t MOVE_POWER = 50;

struct Thruster {
  int pin1;
  int pin2;

  void init(int pin1_, int pin2_) {
    pin1 = pin1_;
    pin2 = pin2_;
    Palatis::SoftPWM.set(pin1, 0);
    Palatis::SoftPWM.set(pin2, 0);
  }

  void setPower(int power) {
    power = constrain(power, -100, 100);

    if (power >= 0) {
      Palatis::SoftPWM.set(pin1, power);
      Palatis::SoftPWM.set(pin2, 0);
      return;
    }

    Palatis::SoftPWM.set(pin1, 0);
    Palatis::SoftPWM.set(pin2, -power);
  }
};

Thruster leftMotor;
Thruster rightMotor;
Thruster verticalMotor;

char current_command = 'S';
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;

void set_motors(int8_t left, int8_t right, int8_t vertical) {
  leftMotor.setPower(left);
  rightMotor.setPower(right);
  verticalMotor.setPower(vertical);
}

void apply_current_command() {
  switch (current_command) {
    case 'F':
      set_motors(MOVE_POWER, MOVE_POWER, 0);
      break;
    case 'B':
      set_motors(-MOVE_POWER, -MOVE_POWER, 0);
      break;
    case 'L':
      set_motors(MOVE_POWER, -MOVE_POWER, 0);
      break;
    case 'R':
      set_motors(-MOVE_POWER, MOVE_POWER, 0);
      break;
    case 'S':
    case 'D':
    default:
      set_motors(0, 0, 0);
      break;
  }
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
      apply_current_command();
    }
  }
}

void stop_if_command_lost() {
  if (current_command != 'S' && millis() - last_command_ms > COMMAND_TIMEOUT_MS) {
    current_command = 'S';
    apply_current_command();
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

  Palatis::SoftPWM.begin(1000);

  pinMode(EMG_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  leftMotor.init(8, 9);
  rightMotor.init(10, 11);
  verticalMotor.init(12, 13);

  current_command = 'S';
  apply_current_command();
  last_command_ms = millis();
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  send_emg_sample();
}
