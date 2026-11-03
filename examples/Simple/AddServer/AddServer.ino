// SPDX-License-Identifier: Apache-2.0
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
