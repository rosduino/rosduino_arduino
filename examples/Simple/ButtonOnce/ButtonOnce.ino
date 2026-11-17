// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_msgs/msg/Bool.h>

const uint8_t kButtonPin = 28;

std_msgs::Bool pressed;
bool was;
rosduino::Timer timer(50);   // every 50 ms

void setup() {
  pinMode(kButtonPin, INPUT);
  was = digitalRead(kButtonPin);
  ros.begin("button");
  ros.advertise("button", pressed);
}

void loop() {
  ros.update();
  if (timer.check()) {
    pressed.data = digitalRead(kButtonPin);
    if (pressed.data != was) {
      was = pressed.data;
      ros.publish("button", pressed);
    }
  }
}
