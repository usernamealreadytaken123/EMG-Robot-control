const uint32_t SERIAL_BAUD = 115200;
const uint8_t EMG_PIN = A0;
const uint16_t SAMPLE_PERIOD_MS = 20;

uint32_t last_sample_ms = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(EMG_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  uint32_t now = millis();
  if (now - last_sample_ms < SAMPLE_PERIOD_MS) {
    return;
  }

  last_sample_ms = now;
  Serial.println(analogRead(EMG_PIN));
}
