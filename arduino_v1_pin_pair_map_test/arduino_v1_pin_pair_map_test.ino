const uint32_t SERIAL_BAUD = 115200;

const uint8_t CONTROL_PINS[] = {8, 9, 10, 11, 12, 13};
const uint8_t CONTROL_PIN_COUNT = sizeof(CONTROL_PINS) / sizeof(CONTROL_PINS[0]);

char current_command = 'S';

void all_pins_low() {
  for (uint8_t i = 0; i < CONTROL_PIN_COUNT; i++) {
    digitalWrite(CONTROL_PINS[i], LOW);
  }
}

void set_two_pins(uint8_t first_index, uint8_t second_index) {
  all_pins_low();
  digitalWrite(CONTROL_PINS[first_index], HIGH);
  digitalWrite(CONTROL_PINS[second_index], HIGH);
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (
      command == '0'
      || command == 'S'
      || command == 'D'
      || command == 'A'
      || command == 'B'
      || command == 'C'
      || command == 'E'
      || command == 'F'
      || command == 'G'
      || command == 'H'
      || command == 'I'
      || command == 'J'
      || command == 'K'
      || command == 'L'
      || command == 'M'
      || command == 'N'
      || command == 'O'
      || command == 'P'
    ) {
      current_command = command;
    }
  }
}

void apply_command() {
  switch (current_command) {
    case 'A':
      set_two_pins(0, 1);  // D8 + D9
      break;
    case 'B':
      set_two_pins(0, 2);  // D8 + D10
      break;
    case 'C':
      set_two_pins(0, 3);  // D8 + D11
      break;
    case 'E':
      set_two_pins(0, 4);  // D8 + D12
      break;
    case 'F':
      set_two_pins(0, 5);  // D8 + D13
      break;
    case 'G':
      set_two_pins(1, 2);  // D9 + D10
      break;
    case 'H':
      set_two_pins(1, 3);  // D9 + D11
      break;
    case 'I':
      set_two_pins(1, 4);  // D9 + D12
      break;
    case 'J':
      set_two_pins(1, 5);  // D9 + D13
      break;
    case 'K':
      set_two_pins(2, 3);  // D10 + D11
      break;
    case 'L':
      set_two_pins(2, 4);  // D10 + D12
      break;
    case 'M':
      set_two_pins(2, 5);  // D10 + D13
      break;
    case 'N':
      set_two_pins(3, 4);  // D11 + D12
      break;
    case 'O':
      set_two_pins(3, 5);  // D11 + D13
      break;
    case 'P':
      set_two_pins(4, 5);  // D12 + D13
      break;
    case '0':
    case 'S':
    case 'D':
    default:
      all_pins_low();
      break;
  }
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
  digitalWrite(LED_BUILTIN, current_command == 'S' || current_command == 'D' || current_command == '0' ? LOW : HIGH);
}
