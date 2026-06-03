# Подробное объяснение Arduino-скетча

Файл скетча: `arduino_elementaryrov_emg_control/arduino_elementaryrov_emg_control.ino`.

Этот скетч делает две независимые задачи:

1. Читает ЭМГ с `A0` и отправляет значения в Python строками через USB Serial.
2. Получает от Python символы `S/F/B/L/R` и через MSP-протокол управляет моторами elementaryROV shield.

## Подключение библиотек

```cpp
#include <SoftwareSerial.h>
```

Подключает стандартную Arduino-библиотеку `SoftwareSerial`. Она нужна, чтобы сделать второй последовательный порт на пинах `D9/D10`. USB Serial уже занят связью с Python, поэтому для связи со shield нужен отдельный UART.

```cpp
#include "src/MSP/MSP.h"
```

Подключает официальную библиотеку MSP из проекта ElementaryROV. Она формирует правильные MSP-пакеты для shield. Без неё shield может неправильно понимать команды.

```cpp
MSP msp;
```

Создаёт объект для работы с MSP-протоколом.

```cpp
SoftwareSerial msp_uart(9, 10);
```

Создаёт программный Serial-порт:

```text
D9  = RX Arduino
D10 = TX Arduino
```

Через этот порт Arduino отправляет команды моторам на shield.

## Константы

```cpp
#define MSP_SET_MOTOR 214
```

Код MSP-команды, которая устанавливает значения моторов.

```cpp
const uint32_t SERIAL_BAUD = 115200;
```

Скорость USB Serial между Arduino и Python.

```cpp
const uint32_t MSP_BAUD = 9600;
```

Скорость SoftwareSerial между Arduino и elementaryROV shield.

```cpp
const uint16_t MOTORS_MIN = 1100;
const uint16_t MOTORS_MAX = 1900;
```

Границы PWM-сигнала для моторов. Значения мощности `-100..100` переводятся в диапазон `1100..1900` микросекунд.

```cpp
const uint8_t EMG_PIN = A0;
```

Пин, с которого читается ЭМГ-сигнал.

```cpp
const uint16_t SAMPLE_PERIOD_MS = 20;
```

Arduino отправляет ЭМГ в Python раз в 20 мс, то есть примерно 50 значений в секунду.

```cpp
const uint16_t COMMAND_TIMEOUT_MS = 1000;
```

Если в течение 1 секунды не приходят команды от Python, Arduino вызывает стоп.

```cpp
const uint16_t STOP_LOCK_MS = 0;
```

Обычная задержка после `relax` отключена. Сейчас `relax` не блокирует следующие жесты сам по себе.

```cpp
const uint8_t RELAX_LOCK_COMMANDS = 5;
```

Если Arduino получает 5 команд `S` подряд, включается защитная блокировка.

```cpp
const uint16_t RELAX_COMMAND_LOCK_MS = 5000;
```

Длительность защитной блокировки после 5 команд `S`: 5000 мс, то есть 5 секунд.

```cpp
const uint16_t STOP_HOLD_BURST_PERIOD_MS = 500;
```

Во время защитной блокировки Arduino повторяет стоп-пачку каждые 500 мс.

```cpp
const uint8_t STOP_BURST_COUNT = 6;
```

Одна стоп-пачка состоит из 6 MSP-команд стопа.

```cpp
const uint8_t STOP_BURST_DELAY_MS = 20;
```

Пауза между командами внутри стоп-пачки.

```cpp
const int8_t MOVE_POWER = 45;
```

Мощность движения. Значение `45` означает примерно 45% от диапазона `-100..100`.

## Переменные состояния

```cpp
char current_command = 'S';
```

Хранит текущую команду. По умолчанию `S`, то есть стоп.

```cpp
bool is_armed = false;
```

Флаг разрешения движения. Если `false`, команды движения не должны отправляться моторам.

```cpp
bool msp_started = false;
```

Показывает, был ли уже запущен `SoftwareSerial` для shield. До первой команды движения MSP не стартует, чтобы shield не реагировал при простом включении.

```cpp
uint32_t last_sample_ms = 0;
```

Время последней отправки ЭМГ-сэмпла в Python.

```cpp
uint32_t last_command_ms = 0;
```

Время последней команды, полученной от Python.

```cpp
uint32_t stop_lock_until_ms = 0;
```

Время окончания обычного стоп-окна. Сейчас `STOP_LOCK_MS = 0`, поэтому оно фактически не используется.

```cpp
uint32_t command_lock_until_ms = 0;
```

Время окончания 5-секундной блокировки после 5 команд `relax/S`.

```cpp
uint32_t last_stop_burst_ms = 0;
```

Время последней стоп-пачки.

```cpp
uint8_t consecutive_relax_commands = 0;
```

Счётчик подряд пришедших команд `S`.

```cpp
int8_t target_m1 = 0;
int8_t target_m2 = 0;
int8_t target_m3 = 0;
int8_t target_m4 = 0;
```

Целевые значения четырёх моторов.

```cpp
int8_t sent_m1 = 0;
int8_t sent_m2 = 0;
int8_t sent_m3 = 0;
int8_t sent_m4 = 0;
```

Последние значения, реально отправленные в shield.

```cpp
bool force_motor_update = false;
```

Если `true`, команда моторам будет отправлена даже если значения похожи на старые.

## setup()

```cpp
void setup() {
```

Функция выполняется один раз после включения Arduino.

```cpp
Serial.begin(SERIAL_BAUD);
```

Запускает USB Serial для связи с Python.

```cpp
Serial.println("EMG_ROV_CONTROL_READY");
Serial.println("DISARMED");
```

Отправляет служебные строки. По ним можно понять, что прошит правильный скетч.

```cpp
pinMode(EMG_PIN, INPUT);
```

Настраивает `A0` как вход для ЭМГ.

```cpp
pinMode(9, INPUT);
pinMode(10, INPUT);
```

До запуска MSP держит пины `D9/D10` как входы, чтобы не дёргать shield при включении.

```cpp
pinMode(LED_BUILTIN, OUTPUT);
```

Настраивает встроенный светодиод как выход.

```cpp
set_motor_targets(0, 0, 0, 0);
```

Ставит целевые значения моторов в ноль.

```cpp
for (uint8_t i = 0; i < 4; i++) {
```

Запускает цикл стартового мигания светодиодом.

```cpp
digitalWrite(LED_BUILTIN, HIGH);
delay(150);
digitalWrite(LED_BUILTIN, LOW);
delay(150);
```

Мигает встроенным LED, чтобы показать, что Arduino загрузилась.

## loop()

```cpp
void loop() {
```

Главный цикл, который повторяется постоянно.

```cpp
read_python_command();
```

Читает команды от Python.

```cpp
stop_if_command_lost();
```

Проверяет, не потеряна ли связь с Python.

```cpp
hold_stop_window();
```

Если включено обычное стоп-окно, повторяет стоп-пачку.

```cpp
hold_command_lock();
```

Если включена 5-секундная блокировка после 5 relax, повторяет стоп-пачку.

```cpp
update_motors();
```

Отправляет новую команду моторам, если она нужна.

```cpp
send_emg_sample();
```

Отправляет очередной ЭМГ-сэмпл в Python.

## read_python_command()

```cpp
while (Serial.available() > 0) {
```

Пока в USB Serial есть входящие символы, Arduino их читает.

```cpp
char command = Serial.read();
```

Читает один символ.

```cpp
if (command == '\n' || command == '\r') {
  continue;
}
```

Игнорирует переносы строки.

```cpp
apply_command(command);
```

Передаёт символ в обработчик команд.

## apply_command()

```cpp
last_command_ms = millis();
```

Запоминает время последней команды от Python.

```cpp
if (command == 'D') {
  apply_stop(true);
  return;
}
```

Команда `D` означает аварийный стоп и запрет движения.

```cpp
if (command == 'A') {
  return;
}
```

Команда `A` оставлена для совместимости, сейчас ничего не делает.

```cpp
if (command == 'S') {
  register_relax_command();
  apply_stop(false);
  return;
}
```

Команда `S` означает `relax/stop`: Arduino считает её для защиты и отправляет стоп.

```cpp
if (is_command_locked() && is_move_command(command)) {
  return;
}
```

Если включена 5-секундная блокировка, команды движения игнорируются.

```cpp
if (is_stop_locked() && is_move_command(command)) {
  return;
}
```

Если активно обычное стоп-окно, команды движения тоже игнорируются.

```cpp
if (command == current_command && !force_motor_update) {
  return;
}
```

Повтор одинаковой команды не отправляется в shield лишний раз.

Команды `F/B/L/R` задают разные значения моторов:

```cpp
F -> set_motor_targets(MOVE_POWER, 0, MOVE_POWER, 0)
B -> set_motor_targets(-MOVE_POWER, 0, -MOVE_POWER, 0)
L -> set_motor_targets(MOVE_POWER, 0, -MOVE_POWER, 0)
R -> set_motor_targets(-MOVE_POWER, 0, MOVE_POWER, 0)
```

После каждой команды движения:

```cpp
is_armed = true;
current_command = ...
force_motor_update = true;
```

Это разрешает движение и заставляет Arduino отправить новую MSP-команду.

## Стоп и блокировки

```cpp
apply_stop(bool disarm)
```

Ставит цели моторов в ноль, отправляет стоп-пачку и при необходимости запрещает движение.

```cpp
register_relax_command()
```

Считает подряд пришедшие `S`. После 5 команд включает 5-секундную блокировку.

```cpp
force_locked_stop()
```

Срабатывает при 5 relax подряд: ставит моторы в ноль, запрещает движение и отправляет стоп-пачку.

```cpp
hold_command_lock()
```

Пока активна 5-секундная блокировка, каждые 500 мс повторяет стоп-пачку.

## Отправка моторов

```cpp
power_to_useconds()
```

Переводит мощность `-100..100` в PWM `1100..1900`.

```cpp
send_motor_values()
```

Формирует массив из 8 моторных каналов и отправляет его через:

```cpp
msp.send(MSP_SET_MOTOR, data, 16);
```

Первые 4 канала используются для моторов робота. Остальные 4 всегда отправляются как ноль.

```cpp
send_stop_burst()
```

Отправляет несколько команд стопа подряд. Это нужно, чтобы shield надёжнее получил остановку.
