# RosDuino example sketches

[Русский](examples_RU.md)

Each example is a sketch at `examples/<Simple|Full>/<Name>/<Name>.ino`
(Arduino IDE: **File → Examples → RosDuino**).

- **Simple API** (`examples/Simple/`) — the global `ros` object and plain strings. The message type is written once, in the callback or the variable. Works on every board, including Nano.
- **Full API** (`examples/Full/`) — `Publisher` / `Subscriber` objects, QoS, names in flash via `F()`, and explicit buffer sizes. For Mega and Due, larger projects, and saving RAM on Nano.
  On AVR, `F()` cannot be used in a global initializer (`PSTR` is a statement-expression). Set flash names in `setup()`.

Both APIs share one engine and can be mixed in one sketch.

## Setup (every example)

1. Flash the example (Arduino IDE or `arduino-cli`).
2. Start the agent. It connects the board to ROS 2 (native install or Docker, below).
3. Run the commands in each example's "On the computer" section (`ros2 topic echo`, `ros2 topic pub`, …).

**Native ROS 2** (sourced workspace, Linux with `/dev/ttyUSB0` / `/dev/ttyACM*`):

```bash
ros2 run rosduino_agent rosduino_agent --port /dev/ttyUSB0
```

Run the "On the computer" commands in a second terminal with the same ROS 2 environment.

**Docker** (macOS, and any host without ROS 2): `rosduino_agent` runs in a container. Native ROS 2 on macOS is not supported.
Stop the agent before the next upload. It holds the port.

Until the agent connects, `ros.connected()` is `false` and `ros.publish()` sends nothing (`rosduino::Result::NotConnected`).

---

## Simple API

### Simple/Led — turn the LED on from the computer (subscribe)

The board listens on `/led` (`std_msgs/msg/Bool`): `true` turns the LED on, `false` turns it off.
Nothing is wired. The sketch uses the onboard `LED_BUILTIN`.

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

On the computer:

```bash
ros2 topic pub --once /led std_msgs/msg/Bool "{data: true}"
ros2 topic pub --once /led std_msgs/msg/Bool "{data: false}"
```

What matters here:
- `ros.begin("led_board")` is the board's name in ROS (node `/led_board`). Port `Serial`, baud 115200.
  Another port or baud: see `Simple/LedSerial1`.
- The callback sets the topic type. `onLed` takes `std_msgs::Bool`, so `/led` is `std_msgs/msg/Bool`.
- Call `ros.update()` on every `loop()`, and do not block in a long `delay()`. Otherwise incoming messages are not read.

### Simple/LightSensor — a light sensor (publish)

Ten times a second the board sends the brightness of a photoresistor on pin `A0` to `/light` (`std_msgs/msg/Int32`, 0–1023).
Wiring: photoresistor between `5V` and `A0`, a 10 kΩ resistor between `A0` and `GND`.

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

On the computer:

```bash
ros2 topic echo /light
ros2 topic hz /light
```

What matters here:
- `rosduino::Timer timer(100)` plus `timer.check()` asks "is it time?" every 100 ms, without `millis()` or `delay()`.
  Two intervals need two timers.
- The variable sets the topic type: `std_msgs::Int32` means `std_msgs/msg/Int32`.
- The first `ros.publish("light", ...)` registers the topic. Messages are not sent until that registration is confirmed.

### Simple/LedService — turn the LED on by request (service on the board)

The board offers `/set_led` (`std_srvs/srv/SetBool`). The computer asks, the board acts and answers.

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

On the computer:

```bash
ros2 service call /set_led std_srvs/srv/SetBool "{data: true}"
```

What matters here:
- The callback receives one service object: `srv.request` is the question, `srv.response` is the answer.
  The `&` in `std_srvs::SetBool& srv` is what lets the callback fill in the response.
- The response is sent when the callback returns. Keep the callback short. While it runs, the board does not read the port.

### Simple/AddServer — add two numbers on the board (service on the board)

The board offers `/add_two_ints` (`example_interfaces/srv/AddTwoInts`). The computer sends two numbers, the board adds them and replies.

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

On the computer:

```bash
ros2 service call /add_two_ints example_interfaces/srv/AddTwoInts "{a: 2, b: 3}"
```

What matters here:
- Same shape as `LedService`: one object in the callback. `srv.request.a` and `srv.request.b` are the addends, `srv.response.sum` is the sum.
- This sketch is the pair of `AddClient`: the same service `/add_two_ints`, but the board does the arithmetic.

### Simple/AddClient — ask the computer to add two numbers (call a service from the board)

Every 2 seconds the board asks `/add_two_ints` (`example_interfaces/srv/AddTwoInts`) to add 2 and 3.
If the answer is 5, the LED turns on.

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

On the computer (the server that adds):

```bash
ros2 run demo_nodes_cpp add_two_ints_server
```

What matters here:
- `adder` holds both the question (`request`) and the answer (`response`). Fill the request, call `ros.call(...)`, and the answer arrives later in `onAnswer`. `loop()` keeps running in between.
- While a call is in flight, another `ros.call()` for the same service returns `rosduino::Result::Busy` and sends nothing.
- If the server does not answer within 1 s, the callback is not called and `ros.call()` can be used again. The simple API sets that timeout with `setCallTimeout(1000)`.

### Simple/LedSerial1 — agent on Serial1, Serial left for debug (Micro, Mega, Due)

The same sketch as `Simple/Led`, but the agent uses the second hardware port `Serial1`. `Serial` (USB) stays free, so the Serial Monitor can print what arrived from the computer.
Wiring: a USB-UART adapter (3.3 V for Due). Adapter `TX` to `RX1`, adapter `RX` to `TX1`, `GND` to `GND`.
On Micro those pins are `0` (`RX`) and `1` (`TX`). On Mega and Due they are `19` (`RX1`) and `18` (`TX1`).
Start the agent on the adapter's port. Open the Serial Monitor on the board's port.

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

On the computer (the adapter port, for example `/dev/ttyUSB0`):

```bash
ros2 run rosduino_agent rosduino_agent --port /dev/ttyUSB0
ros2 topic pub --once /led std_msgs/msg/Bool "{data: true}"
```

What matters here:
- The port is chosen in `ros.begin()`. The library calls `begin()` on that port:

  | Call | Port | Baud |
  |---|---|---|
  | `ros.begin("led_board")` | `Serial` | 115200 |
  | `ros.begin(Serial1, "led_board")` | `Serial1` | 115200 |
  | `ros.begin(Serial1, 57600, "led_board")` | `Serial1` | 57600 |

  The baud in the sketch and the agent's `--baud` must match. The agent default is also 115200.
- Any hardware port on the board works: `Serial1` on Micro, Mega, and Due; `Serial2` and `Serial3` on Mega and Due; `SerialUSB` on Due. Nano and Uno have only `Serial`, so this example does not compile there.
- A stream the sketch already opened is attached without a second `begin()`: `ros.attach(port, "led_board")`.

---

## Full API

### Full/Blink — toggle the LED

`Simple/Led` on the full API: an explicit node, a subscriber object, and names in flash (`F()`), which uses less RAM on Nano.
`/toggle_led` (`std_msgs/msg/Empty`) toggles the LED.

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

On the computer: `ros2 topic pub --once /toggle_led std_msgs/msg/Empty "{}"`

### Full/ServiceServer — a counter and a service

Publish with QoS `sensorData()`, and a service callback of `(Request, Response)`.

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

### Full/ServiceClient — an asynchronous call and a blocking call

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

### Full/NanoBasic — the Nano load sketch

The Nano load reference: 2 publishers and 1 subscriber.
`/range` is `sensor_msgs/msg/Range` at 10 Hz with `sensorData()`. `/battery_mv` is `std_msgs/msg/Int32` at 1 Hz.
`/cmd_led` is a `std_msgs/msg/Bool` subscription that drives `LED_BUILTIN`.

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

### Full/PubSub — echo an Int32

A `std_msgs/msg/Int32` publisher and a `std_msgs/msg/Int32` subscriber. Each incoming value is incremented by 1 and published back.

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

On the computer: `ros2 topic pub /echo_in std_msgs/msg/Int32 "{data: 1}"` and `ros2 topic echo /echo_out`.

### Full/TimeAndLog — time and logs

`node.now()`, `node.logInfo`, and `node.logWarn` once a second. The logs appear in `ros2 topic echo /rosout`.

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
