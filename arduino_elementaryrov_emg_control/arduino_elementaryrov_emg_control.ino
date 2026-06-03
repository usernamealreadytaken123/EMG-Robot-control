#include <SoftwareSerial.h>      // SoftwareSerial нужен для отдельного UART-канала к elementaryROV shield.
#include "src/MSP/MSP.h"         // Официальная MSP-библиотека ElementaryROV для отправки команд моторам.

MSP msp;                         // Объект MSP-протокола.
SoftwareSerial msp_uart(9, 10);  // Программный UART: D9 = RX, D10 = TX для связи со shield.

#define MSP_SET_MOTOR 214        // Код MSP-команды установки значений моторов.

const uint32_t SERIAL_BAUD = 115200;             // Скорость USB Serial между Arduino и Python.
const uint32_t MSP_BAUD = 9600;                  // Скорость SoftwareSerial между Arduino и shield.
const uint16_t MOTORS_MIN = 1100;                // Минимальная длительность PWM для моторов.
const uint16_t MOTORS_MAX = 1900;                // Максимальная длительность PWM для моторов.
const uint16_t MOTORS_STOP_US = 1500;            // Значение PWM, которое сейчас считаем физическим стопом ESC.

const uint8_t EMG_PIN = A0;                      // Аналоговый вход, куда подключен ЭМГ-сигнал.
const uint16_t SAMPLE_PERIOD_MS = 20;            // Период отправки ЭМГ-сэмплов в Python: 20 мс = 50 Гц.
const uint16_t COMMAND_TIMEOUT_MS = 1000;        // Если команд нет 1 секунду, Arduino уходит в стоп.
const uint16_t STOP_LOCK_MS = 0;                 // Обычная задержка после relax отключена.
const uint8_t RELAX_LOCK_COMMANDS = 5;           // Сколько подряд S/relax нужно для защитной блокировки.
const uint16_t RELAX_COMMAND_LOCK_MS = 5000;     // Длительность защитной блокировки после 5 relax: 5 секунд.
const uint16_t STOP_HOLD_BURST_PERIOD_MS = 500;  // Как часто повторять стоп-пачку во время блокировки.
const uint8_t STOP_BURST_COUNT = 6;              // Сколько стоп-пакетов отправлять за одну стоп-пачку.
const uint8_t STOP_BURST_DELAY_MS = 20;          // Пауза между стоп-пакетами внутри стоп-пачки.
const uint16_t MOTOR_REFRESH_PERIOD_MS = 50;     // Как часто повторять текущую MSP-команду моторов.

const int8_t MOVE_POWER = 20;                    // Мощность движения в процентах от диапазона -100..100.

char current_command = 'S';                      // Текущая команда: S/F/B/L/R.
bool is_armed = false;                           // Разрешены ли команды движения.
bool msp_started = false;                        // Был ли уже запущен SoftwareSerial/MSP.
uint32_t last_sample_ms = 0;                     // Время последней отправки ЭМГ-сэмпла.
uint32_t last_command_ms = 0;                    // Время последней принятой команды от Python.
uint32_t stop_lock_until_ms = 0;                 // Время окончания обычного стоп-окна.
uint32_t command_lock_until_ms = 0;              // Время окончания 5-секундной relax-блокировки.
uint32_t last_stop_burst_ms = 0;                 // Время последней отправленной стоп-пачки.
uint32_t last_motor_refresh_ms = 0;              // Время последней отправленной MSP-команды моторам.
uint8_t consecutive_relax_commands = 0;          // Счётчик подряд пришедших команд S/relax.
int8_t target_m1 = 0;                            // Целевое значение мотора 1.
int8_t target_m2 = 0;                            // Целевое значение мотора 2.
int8_t target_m3 = 0;                            // Целевое значение мотора 3.
int8_t target_m4 = 0;                            // Целевое значение мотора 4.
int8_t sent_m1 = 0;                              // Последнее отправленное значение мотора 1.
int8_t sent_m2 = 0;                              // Последнее отправленное значение мотора 2.
int8_t sent_m3 = 0;                              // Последнее отправленное значение мотора 3.
int8_t sent_m4 = 0;                              // Последнее отправленное значение мотора 4.
bool force_motor_update = false;                 // Флаг принудительной отправки новой моторной команды.

void setup() {                                   // Выполняется один раз после включения или сброса Arduino.
  Serial.begin(SERIAL_BAUD);                     // Запускаем USB Serial для общения с Python.
  Serial.println("EMG_ROV_CONTROL_READY");       // Метка для проверки, что прошит правильный скетч.
  Serial.println("DISARMED");                    // Сообщаем, что движение при старте запрещено.

  pinMode(EMG_PIN, INPUT);                       // Настраиваем A0 как вход ЭМГ.
  pinMode(9, INPUT);                             // До старта MSP держим D9 входом, чтобы не дёргать shield.
  pinMode(10, INPUT);                            // До старта MSP держим D10 входом, чтобы не дёргать shield.
  pinMode(LED_BUILTIN, OUTPUT);                  // Встроенный LED используем как индикатор движения/стопа.

  set_motor_targets(0, 0, 0, 0);                 // При старте целевые значения моторов равны нулю.

  for (uint8_t i = 0; i < 4; i++) {              // Короткое мигание LED при старте скетча.
    digitalWrite(LED_BUILTIN, HIGH);             // Включаем LED.
    delay(150);                                  // Ждём 150 мс.
    digitalWrite(LED_BUILTIN, LOW);              // Выключаем LED.
    delay(150);                                  // Ждём 150 мс.
  }                                              // Завершили стартовое мигание.
}                                                // Конец setup().

void loop() {                                    // Основной цикл Arduino.
  read_python_command();                         // Сначала читаем команды S/F/B/L/R от Python.
  stop_if_command_lost();                        // Проверяем таймаут команд и останавливаемся при потере связи.
  hold_stop_window();                            // Если активно стоп-окно, повторяем стоп-пачку.
  hold_command_lock();                           // Если активна relax-блокировка, повторяем стоп-пачку.
  update_motors();                               // Отправляем команду моторам, если есть новое движение.
  send_emg_sample();                             // Отправляем очередной analogRead(A0) в Python.
}                                                // Конец loop().

void read_python_command() {                     // Читает все доступные символы из USB Serial.
  while (Serial.available() > 0) {               // Пока во входном буфере есть данные.
    char command = Serial.read();                // Читаем один символ команды.

    if (command == '\n' || command == '\r') {    // Игнорируем переводы строк.
      continue;                                  // Переходим к следующему символу.
    }                                            // Конец проверки перевода строки.

    apply_command(command);                      // Обрабатываем команду.
  }                                              // Конец чтения всех доступных символов.
}                                                // Конец read_python_command().

void apply_command(char command) {               // Применяет одну команду от Python.
  last_command_ms = millis();                    // Обновляем время последней команды.

  if (command == 'D') {                          // D = disarm / аварийная остановка.
    apply_stop(true);                            // Останавливаем моторы и запрещаем движение.
    return;                                      // Выходим, команда обработана.
  }                                              // Конец обработки D.

  if (command == 'A') {                          // A оставлено для совместимости со старым hand-shake.
    return;                                      // Сейчас A ничего не делает.
  }                                              // Конец обработки A.

  if (command == 'S') {                          // S = relax / stop.
    register_relax_command();                    // Учитываем очередной relax для защитного счётчика.
    apply_stop(false);                           // Останавливаем моторы, но не обязательно disarm.
    return;                                      // Выходим, команда обработана.
  }                                              // Конец обработки S.

  if (is_command_locked() && is_move_command(command)) {  // Если включена 5-секундная блокировка.
    return;                                      // Игнорируем команды движения.
  }                                              // Конец проверки relax-блокировки.

  if (is_stop_locked() && is_move_command(command)) {     // Если активно обычное стоп-окно.
    return;                                      // Игнорируем команды движения.
  }                                              // Конец проверки стоп-окна.

  if (command == current_command && !force_motor_update) {  // Если команда уже такая же.
    return;                                      // Повторную команду не отправляем в shield.
  }                                              // Конец фильтра повторов.

  switch (command) {                             // Разбираем команду движения.
    case 'F':                                    // F = движение вперёд.
      consecutive_relax_commands = 0;            // Сбрасываем счётчик relax-команд.
      start_msp_if_needed();                     // Запускаем MSP, если он ещё не включён.
      is_armed = true;                           // Разрешаем движение.
      current_command = 'F';                     // Запоминаем текущую команду.
      digitalWrite(LED_BUILTIN, HIGH);           // LED горит, когда есть движение.
      set_motor_targets(MOVE_POWER, 0, MOVE_POWER, 0);  // Задаём моторы для движения вперёд.
      force_motor_update = true;                 // Просим отправить команду в shield.
      break;                                     // Завершили F.
    case 'B':                                    // B = движение назад.
      consecutive_relax_commands = 0;            // Сбрасываем счётчик relax-команд.
      start_msp_if_needed();                     // Запускаем MSP, если нужно.
      is_armed = true;                           // Разрешаем движение.
      current_command = 'B';                     // Запоминаем текущую команду.
      digitalWrite(LED_BUILTIN, HIGH);           // LED горит, когда есть движение.
      set_motor_targets(-MOVE_POWER, 0, -MOVE_POWER, 0);  // Задаём моторы для движения назад.
      force_motor_update = true;                 // Просим отправить команду.
      break;                                     // Завершили B.
    case 'L':                                    // L = поворот/движение влево.
      consecutive_relax_commands = 0;            // Сбрасываем счётчик relax-команд.
      start_msp_if_needed();                     // Запускаем MSP, если нужно.
      is_armed = true;                           // Разрешаем движение.
      current_command = 'L';                     // Запоминаем текущую команду.
      digitalWrite(LED_BUILTIN, HIGH);           // LED горит, когда есть движение.
      set_motor_targets(MOVE_POWER, 0, -MOVE_POWER, 0);  // Один горизонтальный мотор вперёд, другой назад.
      force_motor_update = true;                 // Просим отправить команду.
      break;                                     // Завершили L.
    case 'R':                                    // R = поворот/движение вправо.
      consecutive_relax_commands = 0;            // Сбрасываем счётчик relax-команд.
      start_msp_if_needed();                     // Запускаем MSP, если нужно.
      is_armed = true;                           // Разрешаем движение.
      current_command = 'R';                     // Запоминаем текущую команду.
      digitalWrite(LED_BUILTIN, HIGH);           // LED горит, когда есть движение.
      set_motor_targets(-MOVE_POWER, 0, MOVE_POWER, 0);  // Направление, противоположное L.
      force_motor_update = true;                 // Просим отправить команду.
      break;                                     // Завершили R.
    default:                                     // Любой неизвестный символ.
      apply_stop(false);                         // Безопасно останавливаемся.
      break;                                     // Завершили default.
  }                                              // Конец switch.
}                                                // Конец apply_command().

void stop_if_command_lost() {                    // Следит за потерей связи с Python.
  if (is_armed && millis() - last_command_ms > COMMAND_TIMEOUT_MS) {  // Если давно не было команд.
    apply_stop(true);                            // Останавливаемся и запрещаем движение.
  }                                              // Конец проверки таймаута.
}                                                // Конец stop_if_command_lost().

void send_emg_sample() {                         // Отправляет текущий ЭМГ-сэмпл в Python.
  uint32_t now = millis();                       // Текущее время Arduino.
  if (now - last_sample_ms < SAMPLE_PERIOD_MS) { // Если период 20 мс ещё не прошёл.
    return;                                      // Пока ничего не отправляем.
  }                                              // Конец проверки периода.

  last_sample_ms = now;                          // Запоминаем время отправки.
  Serial.println(analogRead(EMG_PIN));           // Отправляем analogRead(A0) строкой.
}                                                // Конец send_emg_sample().

void stop_motors() {                             // Вспомогательная функция полного стопа.
  apply_stop(true);                              // Останавливаемся и запрещаем движение.
}                                                // Конец stop_motors().

void apply_stop(bool disarm) {                   // Общая функция остановки моторов.
  digitalWrite(LED_BUILTIN, LOW);                // Гасим LED, так как движение остановлено.
  stop_lock_until_ms = millis() + STOP_LOCK_MS;  // Ставим обычное стоп-окно; сейчас оно равно 0 мс.
  set_motor_targets(0, 0, 0, 0);                 // Целевые значения моторов = стоп.

  if (msp_started) {                             // Если shield уже активен после любой команды движения.
    send_stop_burst();                           // Всегда физически отправляем стоп, даже если текущая команда уже S.
  }                                              // Конец отправки стоп-пачки.

  current_command = 'S';                         // Запоминаем, что текущая команда = стоп.
  force_motor_update = false;                    // Сбрасываем флаг принудительной отправки движения.

  if (disarm) {                                  // Если нужен запрет движения.
    is_armed = false;                            // Запрещаем движение.
  }                                              // Конец disarm.
}                                                // Конец apply_stop().

bool is_stop_locked() {                          // Проверяет обычное стоп-окно.
  return (int32_t)(stop_lock_until_ms - millis()) > 0;  // true, пока время окна не вышло.
}                                                // Конец is_stop_locked().

bool is_command_locked() {                       // Проверяет 5-секундную relax-блокировку.
  return (int32_t)(command_lock_until_ms - millis()) > 0;  // true, пока блокировка активна.
}                                                // Конец is_command_locked().

bool is_move_command(char command) {             // Проверяет, является ли команда движением.
  return command == 'F' || command == 'B' || command == 'L' || command == 'R';  // Движение: F/B/L/R.
}                                                // Конец is_move_command().

void register_relax_command() {                  // Считает подряд идущие команды relax/S.
  if (is_command_locked()) {                     // Если 5-секундная блокировка уже активна.
    return;                                      // Не продлеваем её новыми S.
  }                                              // Конец проверки активной блокировки.

  if (consecutive_relax_commands < RELAX_LOCK_COMMANDS) {  // Если счётчик ещё меньше порога.
    consecutive_relax_commands++;                // Увеличиваем счётчик relax.
  }                                              // Конец увеличения счётчика.

  if (consecutive_relax_commands >= RELAX_LOCK_COMMANDS) {  // Если пришло 5 relax подряд.
    command_lock_until_ms = millis() + RELAX_COMMAND_LOCK_MS;  // Включаем блокировку на 5 секунд.
    consecutive_relax_commands = 0;             // Сбрасываем счётчик.
    force_locked_stop();                        // Принудительно останавливаем моторы.
  }                                              // Конец проверки порога.
}                                                // Конец register_relax_command().

void force_locked_stop() {                       // Стоп при входе в 5-секундную relax-блокировку.
  digitalWrite(LED_BUILTIN, LOW);                // Гасим LED.
  set_motor_targets(0, 0, 0, 0);                 // Целевые значения моторов = стоп.
  current_command = 'S';                         // Текущая команда = стоп.
  force_motor_update = false;                    // Сбрасываем отправку движения.
  is_armed = false;                              // Запрещаем новые движения.

  if (msp_started) {                             // Если MSP уже был запущен.
    send_stop_burst();                           // Отправляем стоп-пачку.
  }                                              // Конец отправки стоп-пачки.
}                                                // Конец force_locked_stop().

void hold_stop_window() {                        // Поддерживает обычное стоп-окно.
  if (!is_stop_locked() || !msp_started) {       // Если окно не активно или MSP не запущен.
    return;                                      // Ничего не делаем.
  }                                              // Конец проверки.

  if (millis() - last_stop_burst_ms >= STOP_HOLD_BURST_PERIOD_MS) {  // Если пора повторить стоп.
    send_stop_burst();                           // Повторяем стоп-пачку.
  }                                              // Конец повторения стоп-пачки.
}                                                // Конец hold_stop_window().

void hold_command_lock() {                       // Поддерживает 5-секундную relax-блокировку.
  if (!is_command_locked() || !msp_started) {    // Если блокировка не активна или MSP не запущен.
    return;                                      // Ничего не делаем.
  }                                              // Конец проверки.

  if (millis() - last_stop_burst_ms >= STOP_HOLD_BURST_PERIOD_MS) {  // Если пора повторить стоп.
    send_stop_burst();                           // Повторяем стоп-пачку, чтобы моторы оставались остановлены.
  }                                              // Конец повторения стоп-пачки.
}                                                // Конец hold_command_lock().

void set_motor_targets(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {  // Запоминает целевые значения моторов.
  target_m1 = m1;                                 // Цель мотора 1.
  target_m2 = m2;                                 // Цель мотора 2.
  target_m3 = m3;                                 // Цель мотора 3.
  target_m4 = m4;                                 // Цель мотора 4.
}                                                // Конец set_motor_targets().

void update_motors() {                            // Отправляет движение в shield, когда нужно.
  bool refresh_due = msp_started && millis() - last_motor_refresh_ms >= MOTOR_REFRESH_PERIOD_MS;  // Проверяем, пора ли повторить MSP-команду.
  bool target_changed = (                         // Проверяем, изменились ли цели моторов.
    target_m1 != sent_m1                          // Мотор 1 изменился.
    || target_m2 != sent_m2                       // Или мотор 2 изменился.
    || target_m3 != sent_m3                       // Или мотор 3 изменился.
    || target_m4 != sent_m4                       // Или мотор 4 изменился.
  );                                              // Конец вычисления target_changed.

  if (is_stop_locked()) {                         // Если активно стоп-окно.
    return;                                      // Выходим без отправки MSP.
  }                                              // Конец проверки отправки.

  if (!is_armed) {                                // Если движение запрещено.
    if (msp_started && refresh_due) {             // Если MSP уже активен и пора повторить безопасный стоп.
      send_stop_values();                         // Повторяем физический стоп, чтобы контроллер не вернулся к старой команде.
    }                                            // Конец периодического стопа.
    return;                                      // Движение не отправляем.
  }                                              // Конец проверки is_armed.

  if (!force_motor_update && !target_changed && !refresh_due) {  // Если нет новой команды и период обновления не вышел.
    return;                                      // Выходим без отправки MSP.
  }                                              // Конец проверки необходимости обновления.

  start_msp_if_needed();                         // Запускаем MSP при первой команде движения.
  send_motor_values(target_m1, target_m2, target_m3, target_m4);  // Отправляем значения моторов.
  force_motor_update = false;                    // Сбрасываем флаг принудительной отправки.
}                                                // Конец update_motors().

void start_msp_if_needed() {                      // Запускает MSP только тогда, когда он реально нужен.
  if (msp_started) {                              // Если MSP уже запущен.
    return;                                      // Ничего не делаем.
  }                                              // Конец проверки.

  msp_uart.begin(MSP_BAUD);                       // Запускаем SoftwareSerial к shield.
  msp.begin(msp_uart);                            // Привязываем MSP-библиотеку к этому порту.
  msp_started = true;                             // Помечаем MSP как запущенный.
}                                                // Конец start_msp_if_needed().

uint16_t power_to_useconds(int8_t power) {        // Переводит мощность -100..100 в PWM-микросекунды.
  power = constrain(power, -100, 100);            // Ограничиваем мощность допустимым диапазоном.
  return map(power, -100, 100, MOTORS_MIN, MOTORS_MAX);  // Преобразуем в 1100..1900 мкс.
}                                                // Конец power_to_useconds().

void send_motor_values(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {  // Отправляет значения моторов через MSP.
  start_msp_if_needed();                         // Гарантируем, что MSP уже запущен.

  uint16_t data[8] = {                            // MSP_SET_MOTOR ждёт 8 каналов моторов.
    power_to_useconds(m1),                        // Канал мотора 1.
    power_to_useconds(m2),                        // Канал мотора 2.
    power_to_useconds(m3),                        // Канал мотора 3.
    power_to_useconds(m4),                        // Канал мотора 4.
    power_to_useconds(0),                         // Остальные каналы держим в нуле.
    power_to_useconds(0),                         // Канал 6 = стоп.
    power_to_useconds(0),                         // Канал 7 = стоп.
    power_to_useconds(0)                          // Канал 8 = стоп.
  };                                              // Конец массива data.

  msp.send(MSP_SET_MOTOR, data, 16);              // Отправляем MSP-команду моторам.

  sent_m1 = m1;                                   // Запоминаем отправленное значение мотора 1.
  sent_m2 = m2;                                   // Запоминаем отправленное значение мотора 2.
  sent_m3 = m3;                                   // Запоминаем отправленное значение мотора 3.
  sent_m4 = m4;                                   // Запоминаем отправленное значение мотора 4.
  last_motor_refresh_ms = millis();               // Запоминаем время отправки MSP-команды.
}                                                // Конец send_motor_values().

void set_motors(int8_t m1, int8_t m2, int8_t m3, int8_t m4) {  // Совместимая обёртка, как в старом скетче.
  send_motor_values(m1, m2, m3, m4);              // Просто отправляем значения моторов.
}                                                // Конец set_motors().

void send_stop_burst() {                          // Отправляет несколько стоп-команд подряд.
  for (uint8_t i = 0; i < STOP_BURST_COUNT; i++) {  // Повторяем стоп несколько раз.
    send_stop_values();                           // Отправляем физический стоп всем моторам.
    delay(STOP_BURST_DELAY_MS);                   // Небольшая пауза между стоп-пакетами.
  }                                              // Конец цикла стоп-пачки.

  last_stop_burst_ms = millis();                  // Запоминаем время последней стоп-пачки.
}                                                // Конец send_stop_burst().

void send_stop_values() {                         // Отправляет одну MSP-команду физического стопа.
  start_msp_if_needed();                          // Гарантируем, что MSP уже запущен.

  uint16_t data[8] = {                            // MSP_SET_MOTOR ждёт 8 каналов моторов.
    MOTORS_STOP_US,                               // Канал мотора 1 = стоп.
    MOTORS_STOP_US,                               // Канал мотора 2 = стоп.
    MOTORS_STOP_US,                               // Канал мотора 3 = стоп.
    MOTORS_STOP_US,                               // Канал мотора 4 = стоп.
    MOTORS_STOP_US,                               // Канал 5 = стоп.
    MOTORS_STOP_US,                               // Канал 6 = стоп.
    MOTORS_STOP_US,                               // Канал 7 = стоп.
    MOTORS_STOP_US                                // Канал 8 = стоп.
  };                                              // Конец массива data.

  msp.send(MSP_SET_MOTOR, data, 16);              // Отправляем MSP-команду физического стопа.

  sent_m1 = 0;                                    // Запоминаем логическое значение мотора 1 как стоп.
  sent_m2 = 0;                                    // Запоминаем логическое значение мотора 2 как стоп.
  sent_m3 = 0;                                    // Запоминаем логическое значение мотора 3 как стоп.
  sent_m4 = 0;                                    // Запоминаем логическое значение мотора 4 как стоп.
  last_motor_refresh_ms = millis();               // Запоминаем время отправки MSP-стопа.
}                                                // Конец send_stop_values().
