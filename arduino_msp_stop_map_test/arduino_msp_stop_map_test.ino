#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_MOTOR 214

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t STOP_US = 1500;
const uint16_t RUN_US = 1560;
const uint8_t REFRESH_DELAY_MS = 5;

char current_command = 'S';

void send_raw_motors(uint16_t values[8]) {
  msp.send(MSP_SET_MOTOR, values, 16);
}

void stop_all() {
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

  send_raw_motors(data);
}

void run_all_except(uint8_t stopped_channel) {
  uint16_t data[8] = {
    RUN_US,
    RUN_US,
    RUN_US,
    RUN_US,
    RUN_US,
    RUN_US,
    RUN_US,
    RUN_US
  };

  if (stopped_channel >= 1 && stopped_channel <= 8) {
    data[stopped_channel - 1] = STOP_US;
  }

  send_raw_motors(data);
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (
      command == 'A'
      || command == 'S'
      || command == 'D'
      || (command >= '1' && command <= '8')
    ) {
      current_command = command;
    }
  }
}

void apply_current_command() {
  if (current_command == 'A') {
    run_all_except(0);
    return;
  }

  if (current_command >= '1' && current_command <= '8') {
    run_all_except(current_command - '0');
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
