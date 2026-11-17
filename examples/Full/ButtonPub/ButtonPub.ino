// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_msgs/msg/Bool.h>

const uint8_t kButtonPin = 28;

rosduino::Node<> node;
rosduino::Publisher<std_msgs::msg::Bool> pub;
std_msgs::msg::Bool pressed;
rosduino::Timer timer(50);   // every 50 ms

void setup() {
  pinMode(kButtonPin, INPUT);
  node.begin(Serial, 115200, F("button"));
  pub = rosduino::Publisher<std_msgs::msg::Bool>(F("button"));
  node.advertise(pub);
}

void loop() {
  node.spinOnce();
  if (timer.check()) {
    pressed.data = digitalRead(kButtonPin);
    pub.publish(pressed);
  }
}
