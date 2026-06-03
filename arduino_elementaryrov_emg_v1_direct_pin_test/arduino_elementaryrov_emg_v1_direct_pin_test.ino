const uint32_t SERIAL_BAUD = 115200;
const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;
const uint16_t COMMAND_TIMEOUT_MS = 1000;

const uint8_t LEFT_FWD_PIN = 8;
const uint8_t LEFT_REV_PIN = 9;
const uint8_t RIGHT_FWD_PIN = 10;
const uint8_t RIGHT_REV_PIN = 11;
const uint8_t VERT_FWD_PIN = 12;
const uint8_t VERT_REV_PIN = 13;

char current_command = 'S';
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;

void set_pair(uint8_t forward_pin, uint8_t reverse_pin, int8_t power) {
  if (power > 0) {
    digitalWrite(forward_pin, HIGH);
    digitalWrite(reverse_pin, LOW);
    return;
  }

  if (power < 0) {
    digitalWrite(forward_pin, LOW);
    digitalWrite(reverse_pin, HIGH);
    return;
  }

  digitalWrite(forward_pin, LOW);
  digitalWrite(reverse_pin, LOW);
}

void set_thrusters(int8_t left, int8_t right, int8_t vertical) {
  set_pair(LEFT_FWD_PIN, LEFT_REV_PIN, left);
  set_pair(RIGHT_FWD_PIN, RIGHT_REV_PIN, right);
  set_pair(VERT_FWD_PIN, VERT_REV_PIN, vertical);
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
      set_thrusters(1, 1, 0);
      break;
    case 'B':
      set_thrusters(-1, -1, 0);
      break;
    case 'L':
      set_thrusters(1, -1, 0);
      break;
    case 'R':
      set_thrusters(-1, 1, 0);
      break;
    case 'S':
    case 'D':
    default:
      set_thrusters(0, 0, 0);
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

  pinMode(EMG_PIN, INPUT);
  pinMode(LEFT_FWD_PIN, OUTPUT);
  pinMode(LEFT_REV_PIN, OUTPUT);
  pinMode(RIGHT_FWD_PIN, OUTPUT);
  pinMode(RIGHT_REV_PIN, OUTPUT);
  pinMode(VERT_FWD_PIN, OUTPUT);
  pinMode(VERT_REV_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  set_thrusters(0, 0, 0);
  last_command_ms = millis();
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  apply_current_command();
  send_emg_sample();
}
