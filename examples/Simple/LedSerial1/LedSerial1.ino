// SPDX-License-Identifier: Apache-2.0
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
