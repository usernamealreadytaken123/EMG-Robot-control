const uint32_t SERIAL_BAUD = 115200;

const uint8_t CONTROL_PINS[] = {8, 9, 10, 11, 12, 13};
const uint8_t CONTROL_PIN_COUNT = sizeof(CONTROL_PINS) / sizeof(CONTROL_PINS[0]);

char current_command = 'S';

void all_pins_low() {
  for (uint8_t i = 0; i < CONTROL_PIN_COUNT; i++) {
    digitalWrite(CONTROL_PINS[i], LOW);
  }
}

void set_single_pin(uint8_t pin_index) {
  all_pins_low();

  if (pin_index < CONTROL_PIN_COUNT) {
    digitalWrite(CONTROL_PINS[pin_index], HIGH);
  }
}

void read_command() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == '\n' || command == '\r') {
      continue;
    }

    if (command == 'S' || command == 'D' || (command >= '1' && command <= '6')) {
      current_command = command;
    }
  }
}

void apply_command() {
  if (current_command >= '1' && current_command <= '6') {
    set_single_pin(current_command - '1');
    return;
  }

  all_pins_low();
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
