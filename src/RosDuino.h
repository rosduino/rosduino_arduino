// SPDX-License-Identifier: Apache-2.0
#ifndef ROSDUINO_H_
#define ROSDUINO_H_

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif

#include "rosduino/core/cdr.h"
#include "rosduino/core/cobs.h"
#include "rosduino/core/commands.h"
#include "rosduino/core/crc16.h"
#include "rosduino/core/frame.h"
#include "rosduino/core/session.h"

#include <builtin_interfaces/msg/Time.h>

#if !defined(ROSDUINO_PROFILE_LITE) && !defined(ROSDUINO_PROFILE_SMALL) && \
    !defined(ROSDUINO_PROFILE_MEDIUM) && !defined(ROSDUINO_PROFILE_LARGE)
#if defined(ARDUINO_AVR_NANO)
#define ROSDUINO_PROFILE_LITE
#elif defined(ARDUINO_AVR_MICRO) || defined(__AVR_ATmega328P__)
#define ROSDUINO_PROFILE_SMALL
#elif defined(__AVR_ATmega2560__)
#define ROSDUINO_PROFILE_MEDIUM
#elif defined(__SAM3X8E__)
#define ROSDUINO_PROFILE_LARGE
#endif
#endif

#if !defined(ROSDUINO_MAX_ENTITIES)
#if defined(ROSDUINO_PROFILE_LITE)
#define ROSDUINO_MAX_ENTITIES 3
#elif defined(ROSDUINO_PROFILE_SMALL)
#define ROSDUINO_MAX_ENTITIES 8
#elif defined(ROSDUINO_PROFILE_MEDIUM)
#define ROSDUINO_MAX_ENTITIES 24
#elif defined(ROSDUINO_PROFILE_LARGE)
#define ROSDUINO_MAX_ENTITIES 64
#else
#define ROSDUINO_MAX_ENTITIES 16
#endif
#endif

#if !defined(ROSDUINO_RX_SIZE)
#if defined(ROSDUINO_PROFILE_LITE)
#define ROSDUINO_RX_SIZE 96
#elif defined(ROSDUINO_PROFILE_SMALL)
#define ROSDUINO_RX_SIZE 160
#elif defined(ROSDUINO_PROFILE_MEDIUM)
#define ROSDUINO_RX_SIZE 512
#elif defined(ROSDUINO_PROFILE_LARGE)
#define ROSDUINO_RX_SIZE 2048
#else
#define ROSDUINO_RX_SIZE 512
#endif
#endif

#if !defined(ROSDUINO_TX_SIZE)
#if defined(ROSDUINO_PROFILE_LITE)
#define ROSDUINO_TX_SIZE 96
#elif defined(ROSDUINO_PROFILE_SMALL)
#define ROSDUINO_TX_SIZE 160
#elif defined(ROSDUINO_PROFILE_MEDIUM)
#define ROSDUINO_TX_SIZE 512
#elif defined(ROSDUINO_PROFILE_LARGE)
#define ROSDUINO_TX_SIZE 2048
#else
#define ROSDUINO_TX_SIZE 512
#endif
#endif

#if !defined(ROSDUINO_DEFAULT_BAUD)
#define ROSDUINO_DEFAULT_BAUD 115200UL
#endif

#include "rosduino/arduino/flash.h"
#include "rosduino/arduino/stream_sink.h"

namespace rosduino {

enum class Result : uint8_t {
  Ok = 0,
  NotConnected,
  NoResources,
  TooLarge,
  Busy,
  InvalidArgument,
  Timeout,
  Unsupported,
};

enum class SpinResult : uint8_t {
  Idle = 0,
  Processed,
  Disconnected,
  Error,
};

class QoS {
 public:
  uint8_t flags;
  uint8_t history_depth;

  QoS() : flags(core::kQosReliable), history_depth(10U) {}

  static QoS defaults() { return QoS(); }

  static QoS sensorData() {
    QoS q;
    q.flags = core::kQosBestEffort;
    q.history_depth = 5U;
    return q;
  }

  QoS& reliable() {
    flags = static_cast<uint8_t>(flags | core::kQosReliable);
    return *this;
  }

  QoS& bestEffort() {
    flags = static_cast<uint8_t>(flags & ~static_cast<uint8_t>(core::kQosReliable));
    return *this;
  }

  QoS& transientLocal() {
    flags = static_cast<uint8_t>(flags | core::kQosTransientLocal);
    return *this;
  }

  QoS& depth(uint8_t n) {
    history_depth = n;
    return *this;
  }
};

class Timer {
 public:
  explicit Timer(uint32_t interval_ms)
      : interval_ms_(interval_ms), last_ms_(0U), started_(false) {}

  bool check() {
    const uint32_t now_ms = millis();
    if (!started_) {
      started_ = true;
      last_ms_ = now_ms;
      return false;
    }
    if (static_cast<uint32_t>(now_ms - last_ms_) >= interval_ms_) {
      last_ms_ = now_ms;
      return true;
    }
    return false;
  }

 private:
  uint32_t interval_ms_;
  uint32_t last_ms_;
  bool started_;
};

template <typename T>
class Publisher {
 public:
  Publisher()
      : name_(nullptr),
        name_flash_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false) {}
  explicit Publisher(const char* name)
      : name_(name),
        name_flash_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false) {}
  explicit Publisher(const __FlashStringHelper* name)
      : name_(nullptr),
        name_flash_(name),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false) {}

  Result publish(const T& msg) {
    if (!bound_ || session_ == nullptr || stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (!session_->connected()) {
      return Result::NotConnected;
    }
    uint8_t* body = session_->writer().beginBody();
    if (body == nullptr) {
      return Result::TooLarge;
    }
    const size_t cap = session_->writer().maxBodySize();
    if (core::CommandCodec::encodeDataHeader(id_, body, cap) != 1U) {
      return Result::TooLarge;
    }
    core::CdrWriter w(&body[1], cap - 1U);
    if (!msg.serialize(w) || !w.ok()) {
      return Result::TooLarge;
    }
    detail::StreamSink sink{stream_};
    if (!session_->sendData(id_, w.size(), millis(), sink)) {
      return Result::NotConnected;
    }
    return Result::Ok;
  }

  const char* name() const { return name_; }
  const __FlashStringHelper* flashName() const { return name_flash_; }
  bool bound() const { return bound_; }
  uint8_t id() const { return id_; }

  core::StrView nameView() const {
    if (name_flash_ != nullptr) {
      return detail::makeFlashStrView(name_flash_);
    }
    return core::StrView::fromCString(name_);
  }

  void bind(core::McuSession* session, Stream* stream, uint8_t id) {
    session_ = session;
    stream_ = stream;
    id_ = id;
    bound_ = true;
  }

 private:
  const char* name_;
  const __FlashStringHelper* name_flash_;
  core::McuSession* session_;
  Stream* stream_;
  uint8_t id_;
  bool bound_;
};

template <typename T>
class Subscriber {
 public:
  Subscriber()
      : name_(nullptr),
        name_flash_(nullptr),
        cb_(nullptr),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        id_(0U),
        bound_(false) {}
  Subscriber(const char* name, void (*cb)(const T&))
      : name_(name),
        name_flash_(nullptr),
        cb_(cb),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        id_(0U),
        bound_(false) {}
  Subscriber(const __FlashStringHelper* name, void (*cb)(const T&))
      : name_(nullptr),
        name_flash_(name),
        cb_(cb),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        id_(0U),
        bound_(false) {}
  Subscriber(const char* name, void (*cb)(const T&, void*), void* ctx)
      : name_(name),
        name_flash_(nullptr),
        cb_(nullptr),
        ctx_cb_(cb),
        ctx_(ctx),
        id_(0U),
        bound_(false) {}
  Subscriber(const __FlashStringHelper* name, void (*cb)(const T&, void*), void* ctx)
      : name_(nullptr),
        name_flash_(name),
        cb_(nullptr),
        ctx_cb_(cb),
        ctx_(ctx),
        id_(0U),
        bound_(false) {}

  const char* name() const { return name_; }
  const __FlashStringHelper* flashName() const { return name_flash_; }
  bool bound() const { return bound_; }
  uint8_t id() const { return id_; }

  core::StrView nameView() const {
    if (name_flash_ != nullptr) {
      return detail::makeFlashStrView(name_flash_);
    }
    return core::StrView::fromCString(name_);
  }

  void bind(uint8_t id) {
    id_ = id;
    bound_ = true;
  }

  static void onData(void* ctx, const uint8_t* cdr, size_t cdr_len) {
    if (ctx != nullptr) {
      static_cast<Subscriber<T>*>(ctx)->dispatch(cdr, cdr_len);
    }
  }

 private:
  void dispatch(const uint8_t* cdr, size_t cdr_len) {
    if (cb_ == nullptr && ctx_cb_ == nullptr) {
      return;
    }
#if defined(ROSDUINO_SUB_STATIC_MSG)
    T& msg = msg_;
#else
    T msg = T();
#endif
    core::CdrReader reader(cdr, cdr_len);
    if (!msg.deserialize(reader) || !reader.ok()) {
      return;
    }
    if (cb_ != nullptr) {
      cb_(msg);
    } else {
      ctx_cb_(msg, ctx_);
    }
  }

  const char* name_;
  const __FlashStringHelper* name_flash_;
  void (*cb_)(const T&);
  void (*ctx_cb_)(const T&, void*);
  void* ctx_;
  uint8_t id_;
  bool bound_;
#if defined(ROSDUINO_SUB_STATIC_MSG)
  T msg_{};
#endif
};

template <typename Srv>
class ServiceServer {
 public:
  using Request = typename Srv::Request;
  using Response = typename Srv::Response;

  ServiceServer()
      : name_(nullptr),
        name_flash_(nullptr),
        cb_(nullptr),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false),
        in_flight_(false) {}
  ServiceServer(const char* name, void (*cb)(const Request&, Response&))
      : name_(name),
        name_flash_(nullptr),
        cb_(cb),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false),
        in_flight_(false) {}
  ServiceServer(const __FlashStringHelper* name, void (*cb)(const Request&, Response&))
      : name_(nullptr),
        name_flash_(name),
        cb_(cb),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false),
        in_flight_(false) {}
  ServiceServer(const char* name, void (*cb)(const Request&, Response&, void*), void* ctx)
      : name_(name),
        name_flash_(nullptr),
        cb_(nullptr),
        ctx_cb_(cb),
        ctx_(ctx),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false),
        in_flight_(false) {}
  ServiceServer(const __FlashStringHelper* name, void (*cb)(const Request&, Response&, void*),
                void* ctx)
      : name_(nullptr),
        name_flash_(name),
        cb_(nullptr),
        ctx_cb_(cb),
        ctx_(ctx),
        session_(nullptr),
        stream_(nullptr),
        id_(0U),
        bound_(false),
        in_flight_(false) {}

  const char* name() const { return name_; }
  const __FlashStringHelper* flashName() const { return name_flash_; }
  bool bound() const { return bound_; }
  uint8_t id() const { return id_; }
  core::StatusCode status() const {
    if (!bound_ || session_ == nullptr) {
      return core::StatusCode::NotInSession;
    }
    return session_->entityStatus(id_);
  }

  core::StrView nameView() const {
    if (name_flash_ != nullptr) {
      return detail::makeFlashStrView(name_flash_);
    }
    return core::StrView::fromCString(name_);
  }

  void bind(core::McuSession* session, Stream* stream, uint8_t id) {
    session_ = session;
    stream_ = stream;
    id_ = id;
    bound_ = true;
  }

  static void onRequest(void* ctx, uint8_t req_id, const uint8_t* cdr, size_t cdr_len) {
    if (ctx != nullptr) {
      static_cast<ServiceServer<Srv>*>(ctx)->dispatch(req_id, cdr, cdr_len);
    }
  }

 private:
  void dispatch(uint8_t req_id, const uint8_t* cdr, size_t cdr_len) {
    if (in_flight_ || session_ == nullptr || stream_ == nullptr ||
        (cb_ == nullptr && ctx_cb_ == nullptr)) {
      return;
    }
    in_flight_ = true;
    Request req = Request();
    Response res = Response();
    core::CdrReader reader(cdr, cdr_len);
    if (req.deserialize(reader) && reader.ok()) {
      if (cb_ != nullptr) {
        cb_(req, res);
      } else {
        ctx_cb_(req, res, ctx_);
      }
    }
    sendResponse(req_id, res);
    in_flight_ = false;
  }

  void sendResponse(uint8_t req_id, const Response& res) {
    if (!bound_ || session_ == nullptr || stream_ == nullptr) {
      return;
    }
    if (!session_->connected()) {
      return;
    }
    uint8_t* body = session_->writer().beginBody();
    if (body == nullptr) {
      return;
    }
    const size_t cap = session_->writer().maxBodySize();
    if (core::CommandCodec::encodeServiceDataHeader(id_, req_id, body, cap) != 2U) {
      return;
    }
    core::CdrWriter w(&body[2], cap - 2U);
    if (!res.serialize(w) || !w.ok()) {
      return;
    }
    detail::StreamSink sink{stream_};
    (void)session_->sendServiceResponse(id_, w.size(), millis(), sink);
  }

  const char* name_;
  const __FlashStringHelper* name_flash_;
  void (*cb_)(const Request&, Response&);
  void (*ctx_cb_)(const Request&, Response&, void*);
  void* ctx_;
  core::McuSession* session_;
  Stream* stream_;
  uint8_t id_;
  bool bound_;
  bool in_flight_;
};

template <typename Srv>
class ServiceClient {
 public:
  using Request = typename Srv::Request;
  using Response = typename Srv::Response;
  using SpinFn = SpinResult (*)(void*);

  ServiceClient()
      : name_(nullptr),
        name_flash_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        spin_(nullptr),
        spin_ctx_(nullptr),
        cb_(nullptr),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        blocking_res_(nullptr),
        id_(0U),
        next_req_id_(1U),
        pending_req_id_(0U),
        bound_(false),
        in_flight_(false),
        blocking_ok_(false),
        timeout_ms_(0U),
        call_ms_(0U) {}
  explicit ServiceClient(const char* name)
      : name_(name),
        name_flash_(nullptr),
        session_(nullptr),
        stream_(nullptr),
        spin_(nullptr),
        spin_ctx_(nullptr),
        cb_(nullptr),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        blocking_res_(nullptr),
        id_(0U),
        next_req_id_(1U),
        pending_req_id_(0U),
        bound_(false),
        in_flight_(false),
        blocking_ok_(false),
        timeout_ms_(0U),
        call_ms_(0U) {}
  explicit ServiceClient(const __FlashStringHelper* name)
      : name_(nullptr),
        name_flash_(name),
        session_(nullptr),
        stream_(nullptr),
        spin_(nullptr),
        spin_ctx_(nullptr),
        cb_(nullptr),
        ctx_cb_(nullptr),
        ctx_(nullptr),
        blocking_res_(nullptr),
        id_(0U),
        next_req_id_(1U),
        pending_req_id_(0U),
        bound_(false),
        in_flight_(false),
        blocking_ok_(false),
        timeout_ms_(0U),
        call_ms_(0U) {}

  Result call(const Request& req, void (*cb)(const Response&)) {
    return startCall(req, cb, nullptr, nullptr, nullptr);
  }

  Result call(const Request& req, void (*cb)(const Response&, void*), void* ctx) {
    return startCall(req, nullptr, cb, ctx, nullptr);
  }

  Result callBlocking(const Request& req, Response& res, uint32_t timeout_ms) {
    const Result sent = startCall(req, nullptr, nullptr, nullptr, &res);
    if (sent != Result::Ok) {
      return sent;
    }
    const uint32_t t0 = millis();
    while (in_flight_) {
      if (static_cast<uint32_t>(millis() - t0) >= timeout_ms) {
        in_flight_ = false;
        blocking_res_ = nullptr;
        return Result::Timeout;
      }
      if (spin_ != nullptr) {
        spin_(spin_ctx_);
      }
    }
    blocking_res_ = nullptr;
    return blocking_ok_ ? Result::Ok : Result::Timeout;
  }

  const char* name() const { return name_; }
  const __FlashStringHelper* flashName() const { return name_flash_; }
  bool bound() const { return bound_; }
  uint8_t id() const { return id_; }
  bool busy() const { return in_flight_ && !timedOut(); }
  void setCallTimeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

  core::StrView nameView() const {
    if (name_flash_ != nullptr) {
      return detail::makeFlashStrView(name_flash_);
    }
    return core::StrView::fromCString(name_);
  }

  void bind(core::McuSession* session, Stream* stream, uint8_t id, SpinFn spin, void* spin_ctx) {
    session_ = session;
    stream_ = stream;
    id_ = id;
    spin_ = spin;
    spin_ctx_ = spin_ctx;
    bound_ = true;
  }

  static void onResponse(void* ctx, uint8_t req_id, const uint8_t* cdr, size_t cdr_len) {
    if (ctx != nullptr) {
      static_cast<ServiceClient<Srv>*>(ctx)->dispatch(req_id, cdr, cdr_len);
    }
  }

 private:
  Result startCall(const Request& req, void (*cb)(const Response&),
                   void (*ctx_cb)(const Response&, void*), void* ctx, Response* blocking_res) {
    if (!bound_ || session_ == nullptr || stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (!session_->connected()) {
      in_flight_ = false;
      return Result::NotConnected;
    }
    if (timedOut()) {
      in_flight_ = false;
    }
    if (in_flight_) {
      return Result::Busy;
    }
    uint8_t* body = session_->writer().beginBody();
    if (body == nullptr) {
      return Result::TooLarge;
    }
    const size_t cap = session_->writer().maxBodySize();
    const uint8_t req_id = next_req_id_;
    next_req_id_ = static_cast<uint8_t>(next_req_id_ + 1U);
    if (next_req_id_ == 0U) {
      next_req_id_ = 1U;
    }
    if (core::CommandCodec::encodeServiceDataHeader(id_, req_id, body, cap) != 2U) {
      return Result::TooLarge;
    }
    core::CdrWriter w(&body[2], cap - 2U);
    if (!req.serialize(w) || !w.ok()) {
      return Result::TooLarge;
    }
    detail::StreamSink sink{stream_};
    if (!session_->sendServiceRequest(id_, w.size(), millis(), sink)) {
      return Result::NotConnected;
    }
    cb_ = cb;
    ctx_cb_ = ctx_cb;
    ctx_ = ctx;
    blocking_res_ = blocking_res;
    pending_req_id_ = req_id;
    blocking_ok_ = false;
    in_flight_ = true;
    call_ms_ = millis();
    return Result::Ok;
  }

  bool timedOut() const {
    return in_flight_ && timeout_ms_ != 0U &&
           static_cast<uint32_t>(millis() - call_ms_) >= timeout_ms_;
  }

  void dispatch(uint8_t req_id, const uint8_t* cdr, size_t cdr_len) {
    if (timedOut()) {
      in_flight_ = false;
    }
    if (!in_flight_ || req_id != pending_req_id_) {
      return;
    }
    in_flight_ = false;
    Response res = Response();
    core::CdrReader reader(cdr, cdr_len);
    if (!res.deserialize(reader) || !reader.ok()) {
      blocking_ok_ = false;
      return;
    }
    blocking_ok_ = true;
    if (blocking_res_ != nullptr) {
      *blocking_res_ = res;
    }
    if (cb_ != nullptr) {
      cb_(res);
    } else if (ctx_cb_ != nullptr) {
      ctx_cb_(res, ctx_);
    }
  }

  const char* name_;
  const __FlashStringHelper* name_flash_;
  core::McuSession* session_;
  Stream* stream_;
  SpinFn spin_;
  void* spin_ctx_;
  void (*cb_)(const Response&);
  void (*ctx_cb_)(const Response&, void*);
  void* ctx_;
  Response* blocking_res_;
  uint8_t id_;
  uint8_t next_req_id_;
  uint8_t pending_req_id_;
  bool bound_;
  bool in_flight_;
  bool blocking_ok_;
  uint32_t timeout_ms_;
  uint32_t call_ms_;
};

namespace detail {

template <typename T>
struct SimpleApiUnsupported {
  static const bool value = false;
};

template <typename Fn>
void* fnPtrAsVoid(Fn fn) {
  static_assert(sizeof(Fn) == sizeof(void*),
                "RosDuino: simple subscribe() needs function pointers the size of void*");
  void* ctx = nullptr;
  memcpy(&ctx, &fn, sizeof(ctx));
  return ctx;
}

template <typename T>
void dispatchSimpleByVal(void* ctx, const uint8_t* cdr, size_t cdr_len) {
  void (*fn)(T) = nullptr;
  memcpy(&fn, &ctx, sizeof(fn));
  if (fn == nullptr) {
    return;
  }
  T msg = T();
  core::CdrReader reader(cdr, cdr_len);
  if (!msg.deserialize(reader) || !reader.ok()) {
    return;
  }
  fn(msg);
}

template <typename T>
void dispatchSimpleByRef(void* ctx, const uint8_t* cdr, size_t cdr_len) {
  void (*fn)(const T&) = nullptr;
  memcpy(&fn, &ctx, sizeof(fn));
  if (fn == nullptr) {
    return;
  }
  T msg = T();
  core::CdrReader reader(cdr, cdr_len);
  if (!msg.deserialize(reader) || !reader.ok()) {
    return;
  }
  fn(msg);
}

template <typename Srv>
struct SimpleServiceThunk {
  static void (*fn)(Srv&);
  static void adapter(const typename Srv::Request& req, typename Srv::Response& res) {
    if (fn == nullptr) {
      return;
    }
    Srv srv = Srv();
    srv.request = req;
    srv.response = res;
    fn(srv);
    res = srv.response;
  }
};

template <typename Srv>
void (*SimpleServiceThunk<Srv>::fn)(Srv&) = nullptr;

template <typename Srv>
struct SimpleClientThunk {
  static Srv* srv;
  static void (*fn)(Srv&);
  static void adapter(const typename Srv::Response& res) {
    if (srv == nullptr) {
      return;
    }
    srv->response = res;
    if (fn != nullptr) {
      fn(*srv);
    }
  }
};

template <typename Srv>
Srv* SimpleClientThunk<Srv>::srv = nullptr;

template <typename Srv>
void (*SimpleClientThunk<Srv>::fn)(Srv&) = nullptr;

}  // namespace detail

template <uint8_t MaxEntities = ROSDUINO_MAX_ENTITIES,
          uint16_t RxSize = ROSDUINO_RX_SIZE,
          uint16_t TxSize = ROSDUINO_TX_SIZE>
class Node {
 public:
  static constexpr uint8_t kMaxEntities = MaxEntities;
  static constexpr uint16_t kRxSize = RxSize;
  static constexpr uint16_t kTxSize = TxSize;
  static constexpr uint16_t kMaxCdrRx = (RxSize > 8U) ? static_cast<uint16_t>(RxSize - 8U) : 0U;
  static constexpr uint16_t kMaxCdrTx = (TxSize > 8U) ? static_cast<uint16_t>(TxSize - 8U) : 0U;

  Node() = default;

  /// Opens the default port (`Serial`, `SerialUSB` with `ROSDUINO_SERIALUSB`) at `ROSDUINO_DEFAULT_BAUD`.
  void begin(const char* node_name) { begin(defaultPort(), ROSDUINO_DEFAULT_BAUD, node_name); }

  void begin(const __FlashStringHelper* node_name) {
    begin(defaultPort(), ROSDUINO_DEFAULT_BAUD, node_name);
  }

  /// Opens `port` (`Serial1`, `Serial2`, ...) at `ROSDUINO_DEFAULT_BAUD`.
  template <typename SerialT>
  void begin(SerialT& port, const char* node_name) {
    begin(port, ROSDUINO_DEFAULT_BAUD, node_name);
  }

  template <typename SerialT>
  void begin(SerialT& port, const __FlashStringHelper* node_name) {
    begin(port, ROSDUINO_DEFAULT_BAUD, node_name);
  }

  /// Opens `port` at `baud`.
  template <typename SerialT>
  void begin(SerialT& port, uint32_t baud, const char* node_name) {
    port.begin(baud);
    start(port, core::StrView::fromCString(node_name));
  }

  template <typename SerialT>
  void begin(SerialT& port, uint32_t baud, const __FlashStringHelper* node_name) {
    port.begin(baud);
    start(port, detail::makeFlashStrView(node_name));
  }

  /// Uses a stream the sketch has already opened; `port.begin()` is not called.
  void attach(Stream& port, const char* node_name) {
    start(port, core::StrView::fromCString(node_name));
  }

  void attach(Stream& port, const __FlashStringHelper* node_name) {
    start(port, detail::makeFlashStrView(node_name));
  }

  SpinResult spinOnce() {
    if (stream_ == nullptr) {
      return SpinResult::Disconnected;
    }
    detail::StreamSink sink{stream_};
    const uint32_t start_ms = millis();
    bool processed = false;

    while (stream_->available() > 0) {
      const int c = stream_->read();
      if (c < 0) {
        break;
      }
      const core::FrameStatus st =
          session_.pushRxByte(static_cast<uint8_t>(c), millis(), sink);
      if (st == core::FrameStatus::FrameReady) {
        processed = true;
      }
      if (static_cast<uint32_t>(millis() - start_ms) >= 4U) {
        break;
      }
    }

    session_.tick(millis(), sink);
    return processed ? SpinResult::Processed : SpinResult::Idle;
  }

  SpinResult update() { return spinOnce(); }

  bool connected() const { return session_.connected(); }

  template <typename T>
  Result advertise(Publisher<T>& pub) {
    return advertise(pub, QoS::defaults());
  }

  template <typename T>
  Result advertise(Publisher<T>& pub, const QoS& qos) {
    static_assert(T::maxSerializedSize() <= kMaxCdrTx,
                  "RosDuino: message maxSerializedSize() exceeds TX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (pub.bound()) {
      return Result::Ok;
    }
    core::EntitySlot spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = core::EntityKind::Publisher;
    spec.qos = qos.flags;
    spec.depth = qos.history_depth;
    spec.name = pub.nameView();
    detail::fillEntityType<T>(&spec);
    uint8_t id = 0U;
    if (!session_.registerEntity(spec, &id)) {
      logError("no entity slots");
      return Result::NoResources;
    }
    pub.bind(&session_, stream_, id);
    return Result::Ok;
  }

  template <typename T>
  Result subscribe(Subscriber<T>& sub) {
    return subscribe(sub, QoS::defaults());
  }

  template <typename T>
  Result subscribe(Subscriber<T>& sub, const QoS& qos) {
    static_assert(T::maxSerializedSize() <= kMaxCdrRx,
                  "RosDuino: message maxSerializedSize() exceeds RX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (sub.bound()) {
      return Result::Ok;
    }
    core::EntitySlot spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = core::EntityKind::Subscriber;
    spec.qos = qos.flags;
    spec.depth = qos.history_depth;
    spec.name = sub.nameView();
    detail::fillEntityType<T>(&spec);
    spec.user_ctx = &sub;
    spec.on_data = &Subscriber<T>::onData;
    uint8_t id = 0U;
    if (!session_.registerEntity(spec, &id)) {
      logError("no entity slots");
      return Result::NoResources;
    }
    sub.bind(id);
    return Result::Ok;
  }

  template <typename Srv>
  Result advertiseService(ServiceServer<Srv>& srv) {
    static_assert(Srv::Request::maxSerializedSize() <= kMaxCdrRx,
                  "RosDuino: service Request maxSerializedSize() exceeds RX CDR budget "
                  "(frame size minus 8)");
    static_assert(Srv::Response::maxSerializedSize() <= kMaxCdrTx,
                  "RosDuino: service Response maxSerializedSize() exceeds TX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (srv.bound()) {
      return Result::Ok;
    }
    core::EntitySlot spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = core::EntityKind::ServiceServer;
    spec.name = srv.nameView();
    detail::fillEntityType<Srv>(&spec);
    spec.user_ctx = &srv;
    spec.on_service = &ServiceServer<Srv>::onRequest;
    uint8_t id = 0U;
    if (!session_.registerEntity(spec, &id)) {
      logError("no entity slots");
      return Result::NoResources;
    }
    srv.bind(&session_, stream_, id);
    return Result::Ok;
  }

  template <typename Srv>
  Result serviceClient(ServiceClient<Srv>& cli) {
    static_assert(Srv::Request::maxSerializedSize() <= kMaxCdrTx,
                  "RosDuino: service Request maxSerializedSize() exceeds TX CDR budget "
                  "(frame size minus 8)");
    static_assert(Srv::Response::maxSerializedSize() <= kMaxCdrRx,
                  "RosDuino: service Response maxSerializedSize() exceeds RX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    if (cli.bound()) {
      return Result::Ok;
    }
    core::EntitySlot spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = core::EntityKind::ServiceClient;
    spec.name = cli.nameView();
    detail::fillEntityType<Srv>(&spec);
    spec.user_ctx = &cli;
    spec.on_service = &ServiceClient<Srv>::onResponse;
    uint8_t id = 0U;
    if (!session_.registerEntity(spec, &id)) {
      logError("no entity slots");
      return Result::NoResources;
    }
    cli.bind(&session_, stream_, id, &Node::spinThunk, this);
    return Result::Ok;
  }

  template <typename Entity>
  Result destroy(Entity& entity) {
    (void)entity;
    return Result::Ok;
  }

  builtin_interfaces::msg::Time now() const {
    builtin_interfaces::msg::Time t;
    t.sec = 0;
    t.nanosec = 0U;
#if !defined(ROSDUINO_NO_TIME)
    int32_t sec = 0;
    uint32_t nsec = 0U;
    session_.syncedTime().getNow(millis(), &sec, &nsec);
    t.sec = sec;
    t.nanosec = nsec;
#endif
    return t;
  }

#if defined(ROSDUINO_NO_LOG)
  void logDebug(const char*) {}
  void logInfo(const char*) {}
  void logWarn(const char*) {}
  void logError(const char*) {}
  void logFatal(const char*) {}
  void logDebug(const __FlashStringHelper*) {}
  void logInfo(const __FlashStringHelper*) {}
  void logWarn(const __FlashStringHelper*) {}
  void logError(const __FlashStringHelper*) {}
  void logFatal(const __FlashStringHelper*) {}
#else
  void logDebug(const char* text) { sendLog(core::LogLevel::Debug, text); }
  void logInfo(const char* text) { sendLog(core::LogLevel::Info, text); }
  void logWarn(const char* text) { sendLog(core::LogLevel::Warn, text); }
  void logError(const char* text) { sendLog(core::LogLevel::Error, text); }
  void logFatal(const char* text) { sendLog(core::LogLevel::Fatal, text); }

  void logDebug(const __FlashStringHelper* text) { sendLog(core::LogLevel::Debug, text); }
  void logInfo(const __FlashStringHelper* text) { sendLog(core::LogLevel::Info, text); }
  void logWarn(const __FlashStringHelper* text) { sendLog(core::LogLevel::Warn, text); }
  void logError(const __FlashStringHelper* text) { sendLog(core::LogLevel::Error, text); }
  void logFatal(const __FlashStringHelper* text) { sendLog(core::LogLevel::Fatal, text); }
#endif

  void onConnect(core::McuSession::ConnectCallback cb, void* ctx = nullptr) {
    session_.setCallbacks(cb, nullptr, ctx);
  }

  void onDisconnect(core::McuSession::ConnectCallback cb, void* ctx = nullptr) {
    session_.setCallbacks(nullptr, cb, ctx);
  }

  template <typename T>
  Result subscribe(const char* topic, void (*fn)(T)) {
    static_assert(sizeof(void (*)(T)) == sizeof(void*),
                  "RosDuino: simple subscribe() needs function pointers the size of void*");
    if (topic == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return subscribeSimple<T>(core::StrView::fromCString(topic), detail::fnPtrAsVoid(fn),
                              &detail::dispatchSimpleByVal<T>);
  }

  template <typename T>
  Result subscribe(const char* topic, void (*fn)(const T&)) {
    static_assert(sizeof(void (*)(const T&)) == sizeof(void*),
                  "RosDuino: simple subscribe() needs function pointers the size of void*");
    if (topic == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return subscribeSimple<T>(core::StrView::fromCString(topic), detail::fnPtrAsVoid(fn),
                              &detail::dispatchSimpleByRef<T>);
  }

  template <typename T>
  Result subscribe(const __FlashStringHelper* topic, void (*fn)(T)) {
    static_assert(sizeof(void (*)(T)) == sizeof(void*),
                  "RosDuino: simple subscribe() needs function pointers the size of void*");
    if (topic == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return subscribeSimple<T>(detail::makeFlashStrView(topic), detail::fnPtrAsVoid(fn),
                              &detail::dispatchSimpleByVal<T>);
  }

  template <typename T>
  Result subscribe(const __FlashStringHelper* topic, void (*fn)(const T&)) {
    static_assert(sizeof(void (*)(const T&)) == sizeof(void*),
                  "RosDuino: simple subscribe() needs function pointers the size of void*");
    if (topic == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return subscribeSimple<T>(detail::makeFlashStrView(topic), detail::fnPtrAsVoid(fn),
                              &detail::dispatchSimpleByRef<T>);
  }

  template <typename Fn>
  Result subscribe(const char* topic, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple subscribe() accepts void(T) or void(const T&); "
                  "use Subscriber<T> for other callbacks");
    (void)topic;
    return Result::InvalidArgument;
  }

  template <typename Fn>
  Result subscribe(const __FlashStringHelper* topic, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple subscribe() accepts void(T) or void(const T&); "
                  "use Subscriber<T> for other callbacks");
    (void)topic;
    return Result::InvalidArgument;
  }

  template <typename T>
  Result publish(const char* topic, const T& msg) {
    if (topic == nullptr) {
      return Result::InvalidArgument;
    }
    return publishSimple<T>(core::StrView::fromCString(topic), msg);
  }

  template <typename T>
  Result publish(const __FlashStringHelper* topic, const T& msg) {
    if (topic == nullptr) {
      return Result::InvalidArgument;
    }
    return publishSimple<T>(detail::makeFlashStrView(topic), msg);
  }

  template <typename Srv>
  Result service(const char* name, void (*fn)(Srv&)) {
    static_assert(sizeof(void (*)(Srv&)) == sizeof(void*),
                  "RosDuino: simple service() needs function pointers the size of void*");
    if (name == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return serviceSimple<Srv>(name, nullptr, fn);
  }

  template <typename Srv>
  Result service(const __FlashStringHelper* name, void (*fn)(Srv&)) {
    static_assert(sizeof(void (*)(Srv&)) == sizeof(void*),
                  "RosDuino: simple service() needs function pointers the size of void*");
    if (name == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return serviceSimple<Srv>(nullptr, name, fn);
  }

  template <typename Fn>
  Result service(const char* name, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple service() accepts void(Srv&); "
                  "use ServiceServer<Srv> for other callbacks");
    (void)name;
    return Result::InvalidArgument;
  }

  template <typename Fn>
  Result service(const __FlashStringHelper* name, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple service() accepts void(Srv&); "
                  "use ServiceServer<Srv> for other callbacks");
    (void)name;
    return Result::InvalidArgument;
  }

  template <typename Srv>
  Result call(const char* name, Srv& srv, void (*fn)(Srv&)) {
    static_assert(sizeof(void (*)(Srv&)) == sizeof(void*),
                  "RosDuino: simple call() needs function pointers the size of void*");
    if (name == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return callSimple<Srv>(name, nullptr, srv, fn);
  }

  template <typename Srv>
  Result call(const __FlashStringHelper* name, Srv& srv, void (*fn)(Srv&)) {
    static_assert(sizeof(void (*)(Srv&)) == sizeof(void*),
                  "RosDuino: simple call() needs function pointers the size of void*");
    if (name == nullptr || fn == nullptr) {
      return Result::InvalidArgument;
    }
    return callSimple<Srv>(nullptr, name, srv, fn);
  }

  template <typename Srv, typename Fn>
  Result call(const char* name, Srv& srv, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple call() accepts void(Srv&); "
                  "use ServiceClient<Srv> for other callbacks");
    (void)name;
    (void)srv;
    return Result::InvalidArgument;
  }

  template <typename Srv, typename Fn>
  Result call(const __FlashStringHelper* name, Srv& srv, Fn, ...) {
    static_assert(detail::SimpleApiUnsupported<Fn>::value,
                  "RosDuino: simple call() accepts void(Srv&); "
                  "use ServiceClient<Srv> for other callbacks");
    (void)name;
    (void)srv;
    return Result::InvalidArgument;
  }

  core::McuSession& session() { return session_; }
  const core::McuSession& session() const { return session_; }

 private:
  void start(Stream& port, const core::StrView& node_name) {
    stream_ = &port;
    session_.attachBuffers(rx_buf_, RxSize, tx_buf_, TxSize, entities_, MaxEntities);
    session_.begin(detail::generateSessionId(), node_name, core::StrView(),
                   detail::defaultHelloFlags(), millis());
  }

#if defined(ROSDUINO_SERIALUSB)
  static decltype(SerialUSB)& defaultPort() { return SerialUSB; }
#else
  // Serial is HardwareSerial on Uno/Mega, UARTClass on Due, and Serial_ (USB CDC) on
  // Leonardo/Micro. The concrete type is required: begin() calls port.begin(baud).
  static decltype(Serial)& defaultPort() { return Serial; }
#endif

  bool findEntity(core::EntityKind kind, const core::StrView& name, uint8_t* out_id) const {
    for (uint8_t i = 0U; i < MaxEntities; ++i) {
      if (entities_[i].reg_state == core::EntityRegState::Unused) {
        continue;
      }
      if (entities_[i].kind != kind) {
        continue;
      }
      if (!detail::strViewEqual(entities_[i].name, name)) {
        continue;
      }
      if (out_id != nullptr) {
        *out_id = i;
      }
      return true;
    }
    return false;
  }

  template <typename T>
  Result subscribeSimple(const core::StrView& name, void* ctx,
                         void (*on_data)(void*, const uint8_t*, size_t)) {
    static_assert(T::maxSerializedSize() <= kMaxCdrRx,
                  "RosDuino: message maxSerializedSize() exceeds RX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    uint8_t id = 0U;
    if (findEntity(core::EntityKind::Subscriber, name, &id)) {
      return Result::Ok;
    }
    core::EntitySlot spec;
    memset(&spec, 0, sizeof(spec));
    spec.kind = core::EntityKind::Subscriber;
    const QoS qos = QoS::defaults();
    spec.qos = qos.flags;
    spec.depth = qos.history_depth;
    spec.name = name;
    detail::fillEntityType<T>(&spec);
    spec.user_ctx = ctx;
    spec.on_data = on_data;
    if (!session_.registerEntity(spec, &id)) {
      logError("no entity slots");
      return Result::NoResources;
    }
    return Result::Ok;
  }

  template <typename Srv>
  Result serviceSimple(const char* name, const __FlashStringHelper* fname, void (*fn)(Srv&)) {
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    detail::SimpleServiceThunk<Srv>::fn = fn;
    const core::StrView nv = (fname != nullptr) ? detail::makeFlashStrView(fname)
                                                : core::StrView::fromCString(name);
    uint8_t id = 0U;
    if (findEntity(core::EntityKind::ServiceServer, nv, &id)) {
      return Result::Ok;
    }
    static ServiceServer<Srv> stored;
    if (fname != nullptr) {
      stored = ServiceServer<Srv>(fname, &detail::SimpleServiceThunk<Srv>::adapter);
    } else {
      stored = ServiceServer<Srv>(name, &detail::SimpleServiceThunk<Srv>::adapter);
    }
    return advertiseService(stored);
  }

  template <typename Srv>
  Result callSimple(const char* name, const __FlashStringHelper* fname, Srv& srv,
                    void (*fn)(Srv&)) {
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    const core::StrView nv = (fname != nullptr) ? detail::makeFlashStrView(fname)
                                                : core::StrView::fromCString(name);
    static ServiceClient<Srv> stored;
    uint8_t id = 0U;
    if (!findEntity(core::EntityKind::ServiceClient, nv, &id)) {
      if (fname != nullptr) {
        stored = ServiceClient<Srv>(fname);
      } else {
        stored = ServiceClient<Srv>(name);
      }
      const Result reg = serviceClient(stored);
      if (reg != Result::Ok) {
        return reg;
      }
      stored.setCallTimeout(1000U);
    }
    if (!session_.connected()) {
      return Result::NotConnected;
    }
    if (stored.busy()) {
      return Result::Busy;
    }
    detail::SimpleClientThunk<Srv>::srv = &srv;
    detail::SimpleClientThunk<Srv>::fn = fn;
    return stored.call(srv.request, &detail::SimpleClientThunk<Srv>::adapter);
  }

  template <typename T>
  Result publishSimple(const core::StrView& name, const T& msg) {
    static_assert(T::maxSerializedSize() <= kMaxCdrTx,
                  "RosDuino: message maxSerializedSize() exceeds TX CDR budget "
                  "(frame size minus 8)");
    if (stream_ == nullptr) {
      return Result::InvalidArgument;
    }
    uint8_t id = 0U;
    if (!findEntity(core::EntityKind::Publisher, name, &id)) {
      core::EntitySlot spec;
      memset(&spec, 0, sizeof(spec));
      spec.kind = core::EntityKind::Publisher;
      const QoS qos = QoS::defaults();
      spec.qos = qos.flags;
      spec.depth = qos.history_depth;
      spec.name = name;
      detail::fillEntityType<T>(&spec);
      if (!session_.registerEntity(spec, &id)) {
        logError("no entity slots");
        return Result::NoResources;
      }
    }
    if (!session_.connected()) {
      return Result::NotConnected;
    }
    uint8_t* body = session_.writer().beginBody();
    if (body == nullptr) {
      return Result::TooLarge;
    }
    const size_t cap = session_.writer().maxBodySize();
    if (core::CommandCodec::encodeDataHeader(id, body, cap) != 1U) {
      return Result::TooLarge;
    }
    core::CdrWriter w(&body[1], cap - 1U);
    if (!msg.serialize(w) || !w.ok()) {
      return Result::TooLarge;
    }
    detail::StreamSink sink{stream_};
    if (!session_.sendData(id, w.size(), millis(), sink)) {
      return Result::NotConnected;
    }
    return Result::Ok;
  }

  static SpinResult spinThunk(void* ctx) {
    return static_cast<Node*>(ctx)->spinOnce();
  }

#if !defined(ROSDUINO_NO_LOG)
  void sendLog(core::LogLevel level, const char* text) {
    if (stream_ == nullptr || text == nullptr) {
      return;
    }
    detail::StreamSink sink{stream_};
    session_.sendLog(level, core::StrView::fromCString(text), millis(), sink);
  }

  void sendLog(core::LogLevel level, const __FlashStringHelper* text) {
    if (stream_ == nullptr || text == nullptr) {
      return;
    }
    detail::StreamSink sink{stream_};
    session_.sendLog(level, detail::makeFlashStrView(text), millis(), sink);
  }
#endif

  Stream* stream_;
  uint8_t rx_buf_[RxSize];
  uint8_t tx_buf_[TxSize];
  core::EntitySlot entities_[MaxEntities];
  core::McuSession session_;
};

}  // namespace rosduino

#if !defined(ROSDUINO_NO_GLOBAL)
rosduino::Node<> ros;
#endif

#endif  // ROSDUINO_H_
