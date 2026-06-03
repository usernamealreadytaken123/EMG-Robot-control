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
const int8_t TEST_POWER = 50;

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

void set_motors(int8_t left, int8_t right, int8_t vertical) {
  leftMotor.setPower(left);
  rightMotor.setPower(right);
  verticalMotor.setPower(vertical);
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (
      command == 'A'
      || command == 'B'
      || command == 'C'
      || command == 'E'
      || command == 'F'
      || command == 'G'
      || command == 'S'
      || command == 'D'
    ) {
      current_command = command;
    }
  }
}

void apply_command() {
  switch (current_command) {
    case 'A':
      set_motors(TEST_POWER, 0, 0);
      break;
    case 'B':
      set_motors(-TEST_POWER, 0, 0);
      break;
    case 'C':
      set_motors(0, TEST_POWER, 0);
      break;
    case 'E':
      set_motors(0, -TEST_POWER, 0);
      break;
    case 'F':
      set_motors(0, 0, TEST_POWER);
      break;
    case 'G':
      set_motors(0, 0, -TEST_POWER);
      break;
    case 'S':
    case 'D':
    default:
      set_motors(0, 0, 0);
      break;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Palatis::SoftPWM.begin(1000);
  pinMode(LED_BUILTIN, OUTPUT);

  leftMotor.init(8, 9);
  rightMotor.init(10, 11);
  verticalMotor.init(12, 13);
  set_motors(0, 0, 0);
}

void loop() {
  read_command();
  apply_command();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
}
