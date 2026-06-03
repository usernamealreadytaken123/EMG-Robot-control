#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t MOTORS_MIN = 1100;
const uint16_t MOTORS_MAX = 1900;
const uint16_t STEP_MS = 4000;
const uint8_t LOOP_DELAY_MS = 5;
const int8_t TEST_POWER = 50;

uint8_t step_index = 0;
uint32_t last_step_ms = 0;

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

void apply_step() {
  switch (step_index) {
    case 0:
      set_motors(0, 0, 0, 0);
      break;
    case 1:
      set_motors(TEST_POWER, 0, TEST_POWER, 0);
      break;
    case 2:
      set_motors(0, 0, 0, 0);
      break;
    case 3:
      set_motors(-TEST_POWER, 0, -TEST_POWER, 0);
      break;
    case 4:
      set_motors(0, 0, 0, 0);
      break;
    case 5:
      set_motors(TEST_POWER, 0, -TEST_POWER, 0);
      break;
    case 6:
      set_motors(0, 0, 0, 0);
      break;
    case 7:
      set_motors(-TEST_POWER, 0, TEST_POWER, 0);
      break;
    case 8:
      set_motors(0, TEST_POWER, 0, TEST_POWER);
      break;
    case 9:
    default:
      set_motors(0, 0, 0, 0);
      break;
  }
}

void print_step() {
  Serial.print("STEP ");
  Serial.print(step_index);
  Serial.print(": ");

  switch (step_index) {
    case 0:
    case 2:
    case 4:
    case 6:
    case 9:
      Serial.println("STOP set_motors(0,0,0,0)");
      break;
    case 1:
      Serial.println("FORWARD set_motors(50,0,50,0)");
      break;
    case 3:
      Serial.println("BACK set_motors(-50,0,-50,0)");
      break;
    case 5:
      Serial.println("LEFT set_motors(50,0,-50,0)");
      break;
    case 7:
      Serial.println("RIGHT set_motors(-50,0,50,0)");
      break;
    case 8:
      Serial.println("VERTICAL set_motors(0,50,0,50)");
      break;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);
  pinMode(LED_BUILTIN, OUTPUT);

  for (uint8_t i = 0; i < 8; i++) {
    set_motors(0, 0, 0, 0);
    digitalWrite(LED_BUILTIN, i % 2 == 0 ? HIGH : LOW);
    delay(500);
  }

  digitalWrite(LED_BUILTIN, LOW);
  last_step_ms = millis();
  print_step();
}

void loop() {
  if (millis() - last_step_ms >= STEP_MS) {
    last_step_ms = millis();
    step_index = (step_index + 1) % 10;
    print_step();
  }

  digitalWrite(LED_BUILTIN, step_index == 0 || step_index == 2 || step_index == 4 || step_index == 6 || step_index == 9 ? LOW : HIGH);
  apply_step();
  delay(LOOP_DELAY_MS);
}
