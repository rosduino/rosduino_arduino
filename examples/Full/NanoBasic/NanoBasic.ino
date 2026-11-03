// SPDX-License-Identifier: Apache-2.0
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
