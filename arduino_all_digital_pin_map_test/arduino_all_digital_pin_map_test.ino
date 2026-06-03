const uint32_t SERIAL_BAUD = 115200;

const uint8_t CONTROL_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const uint8_t CONTROL_PIN_COUNT = sizeof(CONTROL_PINS) / sizeof(CONTROL_PINS[0]);
const char COMMANDS[] = {'A', 'B', 'C', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M'};

char current_command = 'S';

void all_pins_low() {
  for (uint8_t i = 0; i < CONTROL_PIN_COUNT; i++) {
    digitalWrite(CONTROL_PINS[i], LOW);
  }
}

int8_t command_to_index(char command) {
  for (uint8_t i = 0; i < CONTROL_PIN_COUNT; i++) {
    if (COMMANDS[i] == command) {
      return i;
    }
  }

  return -1;
}

void set_single_pin_by_command(char command) {
  all_pins_low();
  int8_t index = command_to_index(command);

  if (index >= 0) {
    digitalWrite(CONTROL_PINS[index], HIGH);
  }
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (command == 'S' || command == 'D' || command_to_index(command) >= 0) {
      current_command = command;
    }
  }
}

void apply_command() {
  if (current_command == 'S' || current_command == 'D') {
    all_pins_low();
    return;
  }

  set_single_pin_by_command(current_command);
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  for (uint8_t i = 0; i < CONTROL_PIN_COUNT; i++) {
    pinMode(CONTROL_PINS[i], OUTPUT);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  all_pins_low();
}

void loop() {
  read_command();
  apply_command();
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' ? LOW : HIGH);
}
