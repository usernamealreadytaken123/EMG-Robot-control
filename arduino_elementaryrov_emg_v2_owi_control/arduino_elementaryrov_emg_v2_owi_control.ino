#include <SoftwareSerial.h>

SoftwareSerial owi(9, 10);

const uint32_t SERIAL_BAUD = 115200;
const uint32_t OWI_BAUD = 1200;
const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;
const uint16_t COMMAND_PERIOD_MS = 50;
const uint16_t COMMAND_TIMEOUT_MS = 1000;
const int8_t MOVE_POWER = 50;

char current_command = 'S';
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;
uint32_t last_owi_command_ms = 0;

unsigned char crc8(uint8_t *pcBlock, unsigned int len) {
  unsigned char crc = 0xFF;

  while (len--) {
    crc ^= *pcBlock++;

    for (unsigned int i = 0; i < 8; i++) {
      crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
    }
  }

  return crc;
}

void drifting_zero(int8_t &val) {
  static const int range = 5;
  if (abs(val) < 15) {
    val = -(millis() % range);
  }
}

void send_owi_data(int8_t left, int8_t vert1, int8_t right, int8_t vert2, int8_t add, bool use_drifting_zero) {
  int8_t data[] = {
    static_cast<int8_t>(0xAA),
    static_cast<int8_t>(0xEE),
    left,
    vert1,
    right,
    vert2,
    add,
    0,
    static_cast<int8_t>(0xEF)
  };

  if (use_drifting_zero) {
    for (uint8_t i = 2; i <= 5; i++) {
      drifting_zero(data[i]);
    }
  }

  data[7] = static_cast<int8_t>(crc8(reinterpret_cast<uint8_t *>(data + 2), 5));
  owi.write(reinterpret_cast<uint8_t *>(data), 9);
}

void send_owi_data(int8_t left, int8_t vert1, int8_t right, int8_t vert2, int8_t add) {
  send_owi_data(left, vert1, right, vert2, add, true);
}

void send_hard_stop() {
  send_owi_data(0, 0, 0, 0, 0, false);
}

void ping_motors() {
  for (int8_t i = -10; i <= 10; i++) {
    send_owi_data(i, i, i, i, 0);
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
    }
  }
}

void stop_if_command_lost() {
  if (current_command != 'S' && millis() - last_command_ms > COMMAND_TIMEOUT_MS) {
    current_command = 'S';
  }
}

void send_current_owi_command() {
  uint32_t now = millis();
  if (now - last_owi_command_ms < COMMAND_PERIOD_MS) {
    return;
  }

  last_owi_command_ms = now;

  switch (current_command) {
    case 'F':
      send_owi_data(MOVE_POWER, 0, MOVE_POWER, 0, 0);
      break;
    case 'B':
      send_owi_data(-MOVE_POWER, 0, -MOVE_POWER, 0, 0);
      break;
    case 'L':
      send_owi_data(MOVE_POWER, 0, -MOVE_POWER, 0, 0);
      break;
    case 'R':
      send_owi_data(-MOVE_POWER, 0, MOVE_POWER, 0, 0);
      break;
    case 'S':
    case 'D':
    default:
      send_hard_stop();
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
  owi.begin(OWI_BAUD);
  pinMode(EMG_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  delay(2200);
  ping_motors();
  last_command_ms = millis();
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  send_current_owi_command();
  send_emg_sample();
}
