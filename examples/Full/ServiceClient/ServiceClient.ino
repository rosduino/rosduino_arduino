// SPDX-License-Identifier: Apache-2.0
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
