// SPDX-License-Identifier: Apache-2.0
#ifndef ROSDUINO_ARDUINO_FLASH_H_
#define ROSDUINO_ARDUINO_FLASH_H_

#include <Arduino.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

#include "rosduino/core/commands.h"
#include "rosduino/core/session.h"

namespace rosduino {
namespace detail {

inline core::StrView makeFlashStrView(const __FlashStringHelper* fstr) {
  if (fstr == nullptr) {
    return core::StrView(nullptr, 0U, true);
  }
  const char* p = reinterpret_cast<const char*>(fstr);
#if defined(__AVR__)
  const size_t n = strlen_P(p);
  const uint8_t clamped = (n > 255U) ? 255U : static_cast<uint8_t>(n);
  return core::StrView(p, clamped, true);
#else
  return core::StrView::fromCString(p);
#endif
}

inline uint8_t defaultHelloFlags() {
  uint8_t flags = 0U;
#if defined(ROSDUINO_PROFILE_LITE)
  flags |= core::kFlagProfileLite;
#elif defined(ROSDUINO_PROFILE_SMALL)
  flags |= core::kFlagProfileSmall;
#elif defined(ROSDUINO_PROFILE_MEDIUM)
  flags |= core::kFlagProfileMedium;
#elif defined(ROSDUINO_PROFILE_LARGE)
  flags |= core::kFlagProfileLarge;
#else
  flags |= core::kFlagProfileDefault;
#endif
#if defined(ROSDUINO_NO_TIME)
  flags |= core::kFlagNoTime;
#endif
#if defined(ROSDUINO_NO_LOG)
  flags |= core::kFlagNoLog;
#endif
#if defined(__AVR__)
  flags |= core::kFlagAvrFloat64;
#endif
  return flags;
}

inline uint16_t generateSessionId() {
  const uint16_t s = static_cast<uint16_t>(micros() & 0xFFFFU);
  return (s == 0U) ? 1U : s;
}

inline char strViewCharAt(const core::StrView& v, uint8_t i) {
#if defined(__AVR__)
  if (v.is_progmem) {
    return static_cast<char>(
        pgm_read_byte(reinterpret_cast<const uint8_t*>(v.data) + i));
  }
#endif
  return (v.data != nullptr) ? v.data[i] : '\0';
}

inline bool strViewEqual(const core::StrView& a, const core::StrView& b) {
  if (a.len != b.len) {
    return false;
  }
  for (uint8_t i = 0U; i < a.len; ++i) {
    if (strViewCharAt(a, i) != strViewCharAt(b, i)) {
      return false;
    }
  }
  return true;
}

inline uint64_t loadHashLe(const uint8_t* bytes) {
  if (bytes == nullptr) {
    return 0ULL;
  }
  return static_cast<uint64_t>(bytes[0]) | (static_cast<uint64_t>(bytes[1]) << 8U) |
         (static_cast<uint64_t>(bytes[2]) << 16U) | (static_cast<uint64_t>(bytes[3]) << 24U) |
         (static_cast<uint64_t>(bytes[4]) << 32U) | (static_cast<uint64_t>(bytes[5]) << 40U) |
         (static_cast<uint64_t>(bytes[6]) << 48U) | (static_cast<uint64_t>(bytes[7]) << 56U);
}

template <typename T>
void fillEntityType(core::EntitySlot* spec) {
  spec->type = T::typeName();
  spec->hash_ptr = T::typeHashBytes();
  spec->hash_in_progmem = T::typeHashInProgmem();
#if !defined(__AVR__)
  spec->hash = 0ULL;
  if (spec->hash_ptr != nullptr && !spec->hash_in_progmem) {
    spec->hash = loadHashLe(spec->hash_ptr);
  }
#endif
}

}  // namespace detail
}  // namespace rosduino

#endif  // ROSDUINO_ARDUINO_FLASH_H_
