// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_msgs/msg/Bool.h>

const uint8_t kButtonPin = 28;

std_msgs::Bool pressed;
rosduino::Timer timer(50);   // every 50 ms

void setup() {
  pinMode(kButtonPin, INPUT);
  ros.begin("button");
}

void loop() {
  ros.update();
  if (timer.check()) {
    pressed.data = digitalRead(kButtonPin);
    ros.publish("button", pressed);
  }
}
