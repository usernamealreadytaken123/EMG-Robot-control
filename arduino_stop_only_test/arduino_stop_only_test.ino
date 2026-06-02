#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t MSP_BAUD = 9600;
const uint16_t MOTORS_MIN = 1100;
const uint16_t MOTORS_MAX = 1900;
const uint16_t MOTOR_UPDATE_PERIOD_MS = 20;

uint32_t last_motor_update_ms = 0;

void setup() {
  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  uint32_t now = millis();
  if (now - last_motor_update_ms < MOTOR_UPDATE_PERIOD_MS) {
    return;
  }

  last_motor_update_ms = now;
  set_motors(0, 0, 0, 0);
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
