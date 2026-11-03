// SPDX-License-Identifier: Apache-2.0
#include <RosDuino.h>
#include <std_msgs/msg/Int32.h>

rosduino::Node<> node;
rosduino::Publisher<std_msgs::msg::Int32> pub;
rosduino::Subscriber<std_msgs::msg::Int32> sub;

void onIn(const std_msgs::msg::Int32& msg) {
  std_msgs::msg::Int32 out;
  out.data = msg.data + 1;
  pub.publish(out);
}

void setup() {
  node.begin(Serial, 115200, F("arduino_pubsub"));
  pub = rosduino::Publisher<std_msgs::msg::Int32>(F("echo_out"));
  sub = rosduino::Subscriber<std_msgs::msg::Int32>(F("echo_in"), onIn);
  node.advertise(pub);
  node.subscribe(sub);
}

void loop() { node.spinOnce(); }
