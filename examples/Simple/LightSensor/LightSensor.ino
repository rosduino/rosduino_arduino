// SPDX-License-Identifier: Apache-2.0
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
