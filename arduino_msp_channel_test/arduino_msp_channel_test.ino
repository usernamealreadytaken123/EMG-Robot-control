#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t STOP_US = 1500;
const uint16_t TEST_FORWARD_US = 1580;
const uint16_t TEST_REVERSE_US = 1420;
const uint8_t REFRESH_DELAY_MS = 5;

char current_command = 'S';

void send_raw_motors(
  uint16_t m1,
  uint16_t m2,
  uint16_t m3,
  uint16_t m4,
  uint16_t m5,
  uint16_t m6,
  uint16_t m7,
  uint16_t m8
) {
  uint16_t data[8] = {m1, m2, m3, m4, m5, m6, m7, m8};
  msp.send(MSP_SET_MOTOR, data, 16);
}

void stop_all() {
  send_raw_motors(
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US
  );
}

void run_single_channel(uint8_t channel, uint16_t value_us) {
  uint16_t data[8] = {
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US,
    STOP_US
  };

  if (channel >= 1 && channel <= 8) {
    data[channel - 1] = value_us;
  }

  msp.send(MSP_SET_MOTOR, data, 16);
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (
      command == 'S'
      || command == 'D'
      || (command >= '1' && command <= '8')
      || command == '9'
      || command == '0'
    ) {
      current_command = command;
    }
  }
}

void apply_current_command() {
  if (current_command >= '1' && current_command <= '8') {
    run_single_channel(current_command - '0', TEST_FORWARD_US);
    return;
  }

  if (current_command == '9') {
    run_single_channel(1, TEST_REVERSE_US);
    return;
  }

  if (current_command == '0') {
    run_single_channel(3, TEST_REVERSE_US);
    return;
  }

  stop_all();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);
  pinMode(LED_BUILTIN, OUTPUT);

  for (uint8_t i = 0; i < 8; i++) {
    stop_all();
    digitalWrite(LED_BUILTIN, i % 2 == 0 ? HIGH : LOW);
    delay(500);
  }

  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  read_command();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  apply_current_command();
  delay(REFRESH_DELAY_MS);
}
