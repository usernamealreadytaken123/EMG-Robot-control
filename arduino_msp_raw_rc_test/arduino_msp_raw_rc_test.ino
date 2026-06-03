#include <SoftwareSerial.h>
#include "src/MSP/MSP.h"

MSP msp;
SoftwareSerial msp_uart(9, 10);

#define MSP_SET_RAW_RC 200

const uint32_t SERIAL_BAUD = 115200;
const uint32_t MSP_BAUD = 9600;
const uint16_t RC_LOW = 1400;
const uint16_t RC_NEUTRAL = 1500;
const uint16_t RC_HIGH = 1600;
const uint16_t RC_AUX_LOW = 1000;
const uint16_t RC_AUX_HIGH = 2000;
const uint8_t REFRESH_DELAY_MS = 5;

char current_command = 'S';
bool aux_arm_high = false;

void send_rc_channels(uint16_t ch1, uint16_t ch2, uint16_t ch3, uint16_t ch4) {
  uint16_t data[8] = {
    ch1,
    ch2,
    ch3,
    ch4,
    aux_arm_high ? RC_AUX_HIGH : RC_AUX_LOW,
    RC_NEUTRAL,
    RC_NEUTRAL,
    RC_NEUTRAL
  };

  msp.send(MSP_SET_RAW_RC, data, 16);
}

void send_neutral_rc() {
  send_rc_channels(RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL);
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (command == 'A') {
      aux_arm_high = true;
      current_command = 'S';
      continue;
    }

    if (command == 'D') {
      aux_arm_high = false;
      current_command = 'S';
      continue;
    }

    if (
      command == 'S'
      || command == 'F'
      || command == 'B'
      || command == 'L'
      || command == 'R'
      || (command >= '1' && command <= '8')
    ) {
      current_command = command;
    }
  }
}

void apply_current_command() {
  switch (current_command) {
    case 'F':
      send_rc_channels(RC_NEUTRAL, RC_HIGH, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case 'B':
      send_rc_channels(RC_NEUTRAL, RC_LOW, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case 'L':
      send_rc_channels(RC_LOW, RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case 'R':
      send_rc_channels(RC_HIGH, RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case '1':
      send_rc_channels(RC_HIGH, RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case '2':
      send_rc_channels(RC_NEUTRAL, RC_HIGH, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case '3':
      send_rc_channels(RC_NEUTRAL, RC_NEUTRAL, RC_HIGH, RC_NEUTRAL);
      break;
    case '4':
      send_rc_channels(RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL, RC_HIGH);
      break;
    case '5':
      send_rc_channels(RC_LOW, RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case '6':
      send_rc_channels(RC_NEUTRAL, RC_LOW, RC_NEUTRAL, RC_NEUTRAL);
      break;
    case '7':
      send_rc_channels(RC_NEUTRAL, RC_NEUTRAL, RC_LOW, RC_NEUTRAL);
      break;
    case '8':
      send_rc_channels(RC_NEUTRAL, RC_NEUTRAL, RC_NEUTRAL, RC_LOW);
      break;
    case 'S':
    default:
      send_neutral_rc();
      break;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  msp_uart.begin(MSP_BAUD);
  msp.begin(msp_uart);
  pinMode(LED_BUILTIN, OUTPUT);

  for (uint8_t i = 0; i < 8; i++) {
    send_neutral_rc();
    digitalWrite(LED_BUILTIN, i % 2 == 0 ? HIGH : LOW);
    delay(500);
  }

  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  read_command();
  digitalWrite(LED_BUILTIN, current_command == 'S' ? LOW : HIGH);
  apply_current_command();
  delay(REFRESH_DELAY_MS);
}
