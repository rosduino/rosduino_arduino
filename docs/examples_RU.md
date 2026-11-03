# Примеры скетчей RosDuino

[English](examples.md)

Каждый пример — отдельный скетч в `examples/<Simple|Full>/<Name>/<Name>.ino`
(меню Arduino IDE: **Файл → Примеры → RosDuino**).

- **Простой API** (`examples/Simple/`) — для детей и первого знакомства: готовый объект `ros`, обычные строки,
  тип сообщения пишется один раз. Работает на всех платах, включая Nano.
- **Полный API** (`examples/Full/`) — объекты `Publisher`/`Subscriber`, QoS, имена во flash через `F()`, свои размеры
  буферов. Для Mega/Due, больших проектов и экономии памяти на Nano.
  На AVR макрос `F()` нельзя использовать в инициализаторе глобального объекта (`PSTR` — statement-expression);
  имя во flash задают в `setup()`.

Оба уровня работают на одном движке и смешиваются в одном скетче.

## Подготовка (общая для всех примеров)

1. Прошить пример в плату (Arduino IDE или `arduino-cli`).
2. Запустить агента — он соединяет плату с ROS 2 (нативная установка или Docker, ниже).
3. Выполнять команды из раздела «На компьютере» каждого примера (`ros2 topic echo`, `ros2 topic pub`, …).

**Нативный ROS 2** (sourced workspace, Linux с `/dev/ttyUSB0` / `/dev/ttyACM*`):

```bash
ros2 run rosduino_agent rosduino_agent --port /dev/ttyUSB0
```

Команды «На компьютере» — во втором терминале с тем же ROS 2.

**Docker** (macOS и хост без ROS 2): пакет `rosduino_agent` в контейнере. Нативный ROS 2 на macOS не поддерживается.
Перед следующей прошивкой остановите агент: он держит порт.

Пока агент не подключился, `ros.connected()` возвращает `false`, а `ros.publish()` возвращает `rosduino::Result::NotConnected` и ничего не отправляет.

---

## Простой API

### Simple/Led — включить светодиод с компьютера (подписка)

Плата слушает топик `/led` с типом `std_msgs/msg/Bool`: `true` — светодиод горит, `false` — гаснет.
Подключать ничего не нужно: используется встроенный светодиод `LED_BUILTIN`.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Bool.h>

void onLed(std_msgs::Bool msg) {
  digitalWrite(LED_BUILTIN, msg.data);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ros.begin("led_board");
  ros.subscribe("led", onLed);
}

void loop() {
  ros.update();
}
```

На компьютере:

```bash
ros2 topic pub --once /led std_msgs/msg/Bool "{data: true}"
ros2 topic pub --once /led std_msgs/msg/Bool "{data: false}"
```

Что здесь важно:
- `ros.begin("led_board")` — имя платы в ROS (узел `/led_board`). Порт `Serial`, скорость 115200.
  Другой порт или скорость — см. `Simple/LedSerial1`.
- Тип топика задаёт колбэк: `onLed` принимает `std_msgs::Bool`, значит топик `/led` — `std_msgs/msg/Bool`.
- `ros.update()` должен вызываться в каждом `loop()` и без долгих `delay()`: иначе сообщения не принимаются.

### Simple/LightSensor — датчик света (публикация)

Плата 10 раз в секунду отправляет яркость с фоторезистора на пине `A0` в топик `/light` (`std_msgs/msg/Int32`, 0–1023).
Схема: фоторезистор между `5V` и `A0`, резистор 10 кОм между `A0` и `GND`.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Int32.h>

std_msgs::Int32 light;
rosduino::Timer timer(100);   // every 100 ms

void setup() {
  ros.begin("light_sensor");
}

void loop() {
  ros.update();
  if (timer.check()) {
    light.data = analogRead(A0);
    ros.publish("light", light);
  }
}
```

На компьютере:

```bash
ros2 topic echo /light
ros2 topic hz /light
```

Что здесь важно:
- `rosduino::Timer timer(100)` + `timer.check()` — «пора?» раз в 100 мс, без `millis()` и без `delay()`.
  Для двух разных интервалов — два таймера.
- Тип топика задаёт переменная `light`: `std_msgs::Int32` → `std_msgs/msg/Int32`.
- Первый `ros.publish("light", ...)` сам регистрирует топик; пока регистрация не подтверждена, сообщения не уходят.

### Simple/LedService — включить светодиод по запросу (сервис на плате)

Плата предоставляет сервис `/set_led` с типом `std_srvs/srv/SetBool`: компьютер спрашивает, плата выполняет и отвечает.

```cpp
#include <RosDuino.h>
#include <std_srvs/srv/SetBool.h>

void onSetLed(std_srvs::SetBool& srv) {
  digitalWrite(LED_BUILTIN, srv.request.data);
  srv.response.success = true;
  srv.response.message = "done";
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ros.begin("led_service");
  ros.service("set_led", onSetLed);
}

void loop() {
  ros.update();
}
```

На компьютере:

```bash
ros2 service call /set_led std_srvs/srv/SetBool "{data: true}"
```

Что здесь важно:
- В колбэк приходит один объект сервиса: `srv.request` — что спросили, `srv.response` — что ответить.
  `&` в `std_srvs::SetBool& srv` значит, что ответ можно менять.
- Ответ отправляется, когда колбэк закончился. Колбэк должен быть коротким: пока он работает, плата не принимает данные.

### Simple/AddServer — сложить числа на плате (сервис на плате)

Плата предоставляет сервис `/add_two_ints` (`example_interfaces/srv/AddTwoInts`): компьютер присылает два числа,
плата складывает и отвечает.

```cpp
#include <RosDuino.h>
#include <example_interfaces/srv/AddTwoInts.h>

void onAdd(example_interfaces::AddTwoInts& srv) {
  srv.response.sum = srv.request.a + srv.request.b;
}

void setup() {
  ros.begin("add_server");
  ros.service("add_two_ints", onAdd);
}

void loop() {
  ros.update();
}
```

На компьютере:

```bash
ros2 service call /add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

Что здесь важно:
- Как в `LedService`, в колбэке один объект: `srv.request.a` / `srv.request.b` — слагаемые,
  `srv.response.sum` — сумма.
- Этот пример — пара к `AddClient`: та же услуга `/add_two_ints`, но считает плата, а не компьютер.

### Simple/AddClient — попросить компьютер сложить числа (вызов сервиса с платы)

Раз в 2 секунды плата просит сервис `/add_two_ints` (`example_interfaces/srv/AddTwoInts`) сложить 2 и 3.
Если ответ равен 5 — светодиод загорается.

```cpp
#include <RosDuino.h>
#include <example_interfaces/srv/AddTwoInts.h>

example_interfaces::AddTwoInts adder;
rosduino::Timer timer(2000);  // every 2 s

void onAnswer(example_interfaces::AddTwoInts& srv) {
  digitalWrite(LED_BUILTIN, srv.response.sum == 5);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ros.begin("add_client");
}

void loop() {
  ros.update();
  if (ros.connected() && timer.check()) {
    adder.request.a = 2;
    adder.request.b = 3;
    ros.call("add_two_ints", adder, onAnswer);
  }
}
```

На компьютере (сервер, который складывает):

```bash
ros2 run demo_nodes_cpp add_two_ints_server
```

Что здесь важно:
- `adder` хранит и вопрос (`request`), и ответ (`response`). Заполняем вопрос, вызываем `ros.call(...)`, ответ придёт
  в `onAnswer` позже — `loop()` в это время продолжает работать.
- Пока ответа нет, повторный `ros.call()` с тем же сервисом вернёт `rosduino::Result::Busy` и ничего не отправит.
- Если сервер не отвечает 1 с, колбэк не вызывается, а `ros.call()` снова можно вызывать.

### Simple/LedSerial1 — агент на Serial1, Serial для отладки (Micro, Mega, Due)

Тот же `Simple/Led`, но связь с агентом идёт через второй аппаратный порт `Serial1`. Обычный `Serial` (USB) остаётся
свободным: в Serial Monitor видно, что пришло с компьютера.
Схема: USB-UART адаптер (3,3 В для Due) — `TX` адаптера к `RX1`, `RX` адаптера к `TX1`, `GND` к `GND`.
На Micro это пины `0` (`RX`) и `1` (`TX`), на Mega и Due — пины `19` (`RX1`) и `18` (`TX1`).
Агент запускается на порту адаптера, Serial Monitor — на порту платы.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Bool.h>

void onLed(std_msgs::Bool msg) {
  digitalWrite(LED_BUILTIN, msg.data);
  Serial.println(msg.data ? "led on" : "led off");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  ros.begin(Serial1, "led_board");
  ros.subscribe("led", onLed);
}

void loop() {
  ros.update();
}
```

На компьютере (порт адаптера, например `/dev/ttyUSB0`):

```bash
ros2 run rosduino_agent rosduino_agent --port /dev/ttyUSB0
ros2 topic pub --once /led std_msgs/msg/Bool "{data: true}"
```

Что здесь важно:
- Порт выбирается в `ros.begin()`, библиотека сама вызывает `begin()` у порта:

  | Вызов | Порт | Скорость |
  |---|---|---|
  | `ros.begin("led_board")` | `Serial` | 115200 |
  | `ros.begin(Serial1, "led_board")` | `Serial1` | 115200 |
  | `ros.begin(Serial1, 57600, "led_board")` | `Serial1` | 57600 |

  Скорость в скетче и `--baud` агента должны совпадать (у агента по умолчанию тоже 115200).
- Подходит любой аппаратный порт платы: `Serial1` на Micro, Mega и Due, `Serial2` и `Serial3` на Mega и Due,
  `SerialUSB` на Due. У Nano и Uno есть только `Serial`, на них пример не компилируется.
- Поток, который скетч открыл сам, подключают без повторного `begin()`: `ros.attach(port, "led_board")`.

---

## Полный API

### Full/Blink — переключение светодиода

Аналог `Simple/Led` на полном API: явный объект узла, объект подписчика, имена во flash (`F()`) — меньше RAM на Nano.
Топик `/toggle_led` (`std_msgs/msg/Empty`) переключает светодиод.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Empty.h>

rosduino::Node<> node;
rosduino::Subscriber<std_msgs::msg::Empty> sub;

void onToggle(const std_msgs::msg::Empty&) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  node.begin(Serial, 115200, F("arduino_blink"));
  sub = rosduino::Subscriber<std_msgs::msg::Empty>(F("toggle_led"), onToggle);
  node.subscribe(sub);
}

void loop() { node.spinOnce(); }
```

На компьютере: `ros2 topic pub --once /toggle_led std_msgs/msg/Empty "{}"`

### Full/ServiceServer — счётчик и сервис

Публикация с QoS `sensorData()` и сервис с колбэком `(Request, Response)`.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Int32.h>
#include <std_srvs/srv/SetBool.h>

rosduino::Node<> node;
std_msgs::msg::Int32 counter;
rosduino::Publisher<std_msgs::msg::Int32> pub;
rosduino::ServiceServer<std_srvs::srv::SetBool> srv;

void onSetLed(const std_srvs::srv::SetBool::Request& req, std_srvs::srv::SetBool::Response& res) {
  digitalWrite(LED_BUILTIN, req.data ? HIGH : LOW);
  res.success = true;
  res.message = "ok";                       // const char*, valid until the callback returns
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  node.begin(Serial, 115200, F("arduino_counter"));
  pub = rosduino::Publisher<std_msgs::msg::Int32>(F("counter"));
  srv = rosduino::ServiceServer<std_srvs::srv::SetBool>(F("set_led"), onSetLed);
  node.advertise(pub, rosduino::QoS::sensorData());
  node.advertiseService(srv);
}

void loop() {
  static uint32_t t = 0;
  if (node.connected() && millis() - t >= 100) {
    t = millis();
    counter.data++;
    pub.publish(counter);
  }
  node.spinOnce();
}
```

### Full/ServiceClient — асинхронный и блокирующий вызов

```cpp
#include <RosDuino.h>
#include <example_interfaces/srv/AddTwoInts.h>

rosduino::Node<> node;
rosduino::ServiceClient<example_interfaces::srv::AddTwoInts> add;
example_interfaces::srv::AddTwoInts::Request req;

void onSum(const example_interfaces::srv::AddTwoInts::Response& r) {
  node.logInfo(r.sum == req.a + req.b ? F("sum ok") : F("sum wrong"));
}

void setup() {
  node.begin(Serial, 115200, F("adder_client"));
  add = rosduino::ServiceClient<example_interfaces::srv::AddTwoInts>(F("add_two_ints"));
  node.serviceClient(add);
}

void loop() {
  static uint32_t t = 0;
  if (node.connected() && millis() - t >= 2000) {
    t = millis();
    req.a = 2;
    req.b = 3;
    add.call(req, onSum);  // asynchronous (preferred)
    // example_interfaces::srv::AddTwoInts::Response res;
    // rosduino::Result r = add.callBlocking(req, res, 500);  // blocking, timeout in ms
  }
  node.spinOnce();
}
```

### Full/NanoBasic — типовая нагрузка Nano

Эталон нагрузки Nano: 2 publisher + 1 subscriber.
`/range` — `sensor_msgs/msg/Range` 10 Гц, `sensorData()`; `/battery_mv` — `std_msgs/msg/Int32` 1 Гц;
`/cmd_led` — подписка `std_msgs/msg/Bool` → `LED_BUILTIN`.

```cpp
#include <RosDuino.h>
#include <sensor_msgs/msg/Range.h>
#include <std_msgs/msg/Int32.h>
#include <std_msgs/msg/Bool.h>

rosduino::Node<> node;
sensor_msgs::msg::Range range;
std_msgs::msg::Int32 battery;
rosduino::Publisher<sensor_msgs::msg::Range> rangePub;
rosduino::Publisher<std_msgs::msg::Int32> batteryPub;
rosduino::Subscriber<std_msgs::msg::Bool> ledSub;

void onLed(const std_msgs::msg::Bool& msg) { digitalWrite(LED_BUILTIN, msg.data); }

rosduino::Timer rangeTimer(100);
rosduino::Timer batteryTimer(1000);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  range.radiation_type = sensor_msgs::msg::Range::INFRARED;
  range.min_range = 0.1f;
  range.max_range = 0.8f;
  node.begin(Serial, 115200, F("nano_basic"));
  rangePub = rosduino::Publisher<sensor_msgs::msg::Range>(F("range"));
  batteryPub = rosduino::Publisher<std_msgs::msg::Int32>(F("battery_mv"));
  ledSub = rosduino::Subscriber<std_msgs::msg::Bool>(F("cmd_led"), onLed);
  node.advertise(rangePub, rosduino::QoS::sensorData());
  node.advertise(batteryPub);
  node.subscribe(ledSub);
}

void loop() {
  node.spinOnce();
  if (!node.connected()) return;
  if (rangeTimer.check()) {
    range.header.stamp = node.now();
    range.range = analogRead(A0) * (0.8f / 1023.0f);
    rangePub.publish(range);
  }
  if (batteryTimer.check()) {
    battery.data = analogRead(A1) * 5000L / 1023;
    batteryPub.publish(battery);
  }
}
```

### Full/PubSub — эхо Int32

Publisher `std_msgs/msg/Int32` + subscriber `std_msgs/msg/Int32`: каждое входящее значение увеличивается на 1 и публикуется обратно.

```cpp
#include <RosDuino.h>
#include <std_msgs/msg/Int32.h>

rosduino::Node<> node;
rosduino::Publisher<std_msgs::msg::Int32> pub;
rosduino::Subscriber<std_msgs::msg::Int32> sub;

void onIn(const std_msgs::msg::Int32& msg) {
  std_msgs::msg::Int32 out;
  out.data = msg.data + 1;
  pub.publish(out);
}

void setup() {
  node.begin(Serial, 115200, F("arduino_pubsub"));
  pub = rosduino::Publisher<std_msgs::msg::Int32>(F("echo_out"));
  sub = rosduino::Subscriber<std_msgs::msg::Int32>(F("echo_in"), onIn);
  node.advertise(pub);
  node.subscribe(sub);
}

void loop() { node.spinOnce(); }
```

На компьютере: `ros2 topic pub /echo_in std_msgs/msg/Int32 "{data: 1}"` и `ros2 topic echo /echo_out`.

### Full/TimeAndLog — время и логи

`node.now()` и `node.logInfo` / `node.logWarn` раз в секунду; логи видны в `ros2 topic echo /rosout`.

```cpp
#include <RosDuino.h>

rosduino::Node<> node;
rosduino::Timer timer(1000);

void setup() {
  node.begin(Serial, 115200, F("arduino_time"));
}

void loop() {
  node.spinOnce();
  if (node.connected() && timer.check()) {
    const builtin_interfaces::msg::Time t = node.now();
    node.logInfo(t.sec != 0 ? F("time ok") : F("time unset"));
    node.logWarn(F("demo warn"));
  }
}
```
