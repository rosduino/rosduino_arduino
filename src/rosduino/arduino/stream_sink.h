// SPDX-License-Identifier: Apache-2.0
#ifndef ROSDUINO_ARDUINO_STREAM_SINK_H_
#define ROSDUINO_ARDUINO_STREAM_SINK_H_

#include <Arduino.h>
#include <stdint.h>

namespace rosduino {
namespace detail {

struct StreamSink {
  Stream* stream;
  bool writeByte(uint8_t b) {
    if (stream == nullptr) {
      return false;
    }
    return stream->write(b) == 1U;
  }
};

}  // namespace detail
}  // namespace rosduino

#endif  // ROSDUINO_ARDUINO_STREAM_SINK_H_
