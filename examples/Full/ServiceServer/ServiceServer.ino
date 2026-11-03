// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_msgs/msg/Int32.h>
#include <std_srvs/srv/SetBool.h>

rosduino::Node<> node;
std_msgs::msg::Int32 counter;
rosduino::Publisher<std_msgs::msg::Int32> pub;
rosduino::ServiceServer<std_srvs::srv::SetBool> srv;

void onSetLed(const std_srvs::srv::SetBool::Request& req, std_srvs::srv::SetBool::Response& res) {
  digitalWrite(LED_BUILTIN, req.data ? HIGH : LOW);
  res.success = true;
  res.message = "ok";                       // const char*, valid until the callback returns
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  node.begin(Serial, 115200, F("arduino_counter"));
  pub = rosduino::Publisher<std_msgs::msg::Int32>(F("counter"));
  srv = rosduino::ServiceServer<std_srvs::srv::SetBool>(F("set_led"), onSetLed);
  node.advertise(pub, rosduino::QoS::sensorData());
  node.advertiseService(srv);
}

void loop() {
  static uint32_t t = 0;
  if (node.connected() && millis() - t >= 100) {
    t = millis();
    counter.data++;
    pub.publish(counter);
  }
  node.spinOnce();
}
