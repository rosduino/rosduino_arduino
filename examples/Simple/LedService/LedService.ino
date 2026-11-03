// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_srvs/srv/SetBool.h>

void onSetLed(std_srvs::SetBool& srv) {
  digitalWrite(LED_BUILTIN, srv.request.data);
  srv.response.success = true;
  srv.response.message = "done";
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  ros.begin("led_service");
  ros.service("set_led", onSetLed);
}

void loop() {
  ros.update();
}
