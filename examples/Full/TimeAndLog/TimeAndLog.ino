// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>

rosduino::Node<> node;
rosduino::Timer timer(1000);

void setup() {
  node.begin(Serial, 115200, F("arduino_time"));
}

void loop() {
  node.spinOnce();
  if (node.connected() && timer.check()) {
    const builtin_interfaces::msg::Time t = node.now();
    node.logInfo(t.sec != 0 ? F("time ok") : F("time unset"));
    node.logWarn(F("demo warn"));
  }
}
