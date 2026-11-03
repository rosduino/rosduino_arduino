// SPDX-License-Identifier: Apache-2.0
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
