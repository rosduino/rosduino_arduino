// SPDX-License-Identifier: Apache-2.0
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
