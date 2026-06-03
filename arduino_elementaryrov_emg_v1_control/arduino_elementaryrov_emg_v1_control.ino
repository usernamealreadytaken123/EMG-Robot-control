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

const int8_t MOVE_POWER = 60;
const uint16_t SOFT_PWM_PERIOD_US = 5000;

char current_command = 'S';
int8_t target_left = 0;
int8_t target_right = 0;
int8_t target_vertical = 0;
uint32_t last_sample_ms = 0;
uint32_t last_command_ms = 0;

void set_motor_targets(int8_t left, int8_t right, int8_t vertical) {
  target_left = constrain(left, -100, 100);
  target_right = constrain(right, -100, 100);
  target_vertical = constrain(vertical, -100, 100);
}

void write_motor_pair(uint8_t forward_pin, uint8_t reverse_pin, int8_t power, uint16_t phase_us) {
  power = constrain(power, -100, 100);

  if (power == 0) {
    digitalWrite(forward_pin, LOW);
    digitalWrite(reverse_pin, LOW);
    return;
  }

  uint16_t duty_us = map(abs(power), 0, 100, 0, SOFT_PWM_PERIOD_US);
  bool is_on = phase_us < duty_us;

  if (power > 0) {
    digitalWrite(forward_pin, is_on ? HIGH : LOW);
    digitalWrite(reverse_pin, LOW);
    return;
  }

  digitalWrite(forward_pin, LOW);
  digitalWrite(reverse_pin, is_on ? HIGH : LOW);
}

void update_motors() {
  uint16_t phase_us = micros() % SOFT_PWM_PERIOD_US;
  write_motor_pair(LEFT_FWD_PIN, LEFT_REV_PIN, target_left, phase_us);
  write_motor_pair(RIGHT_FWD_PIN, RIGHT_REV_PIN, target_right, phase_us);
  write_motor_pair(VERT_FWD_PIN, VERT_REV_PIN, target_vertical, phase_us);
}

void apply_current_command() {
  switch (current_command) {
    case 'F':
      set_motor_targets(MOVE_POWER, MOVE_POWER, 0);
      break;
    case 'B':
      set_motor_targets(-MOVE_POWER, -MOVE_POWER, 0);
      break;
    case 'L':
      set_motor_targets(MOVE_POWER, -MOVE_POWER, 0);
      break;
    case 'R':
      set_motor_targets(-MOVE_POWER, MOVE_POWER, 0);
      break;
    case 'S':
    case 'D':
    default:
      set_motor_targets(0, 0, 0);
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

  pinMode(EMG_PIN, INPUT);
  pinMode(LEFT_FWD_PIN, OUTPUT);
  pinMode(LEFT_REV_PIN, OUTPUT);
  pinMode(RIGHT_FWD_PIN, OUTPUT);
  pinMode(RIGHT_REV_PIN, OUTPUT);
  pinMode(VERT_FWD_PIN, OUTPUT);
  pinMode(VERT_REV_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  current_command = 'S';
  apply_current_command();
  update_motors();
  last_command_ms = millis();
}

void loop() {
  read_python_command();
  stop_if_command_lost();
  update_motors();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
  send_emg_sample();
}
