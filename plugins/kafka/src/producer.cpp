//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2023 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "kafka/producer.hpp"

#include <tenzir/detail/narrow.hpp>
#include <tenzir/error.hpp>
#include <tenzir/logger.hpp>

#include <fmt/format.h>

#include <string_view>

namespace tenzir::plugins::kafka {

auto producer::make(configuration config) -> caf::expected<producer> {
  producer result;
  std::string error;
  result.producer_.reset(RdKafka::Producer::create(config.conf_.get(), error));
  if (!result.producer_)
    return caf::make_error(ec::unspecified,
                           fmt::format("failed to create producer: {}", error));
  result.config_ = std::move(config);
  return result;
}

auto producer::produce(std::string topic, std::span<const std::byte> bytes,
                       std::string_view key, time timestamp) -> caf::error {
  auto ms = int64_t{0};
  if (timestamp != time{})
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(
           timestamp.time_since_epoch())
           .count();
  while (true) {
    auto result = producer_->produce(
      /// The message topic.
      std::move(topic),
      // Any partition.
      RdKafka::Topic::PARTITION_UA,
      // Make a copy of the buffer.
      RdKafka::Producer::RK_MSG_COPY,
      // The payload data.
      // Following the call chain, it's unclear why librdkafka uses a non-const
      // pointer here. The pointer should not be mutated. The pointer ends up as
      // payload in rdkafka_msg.c and get's simply copied into rkm_payload:
      //
      //  if (payload && msgflags & RD_KAFKA_MSG_F_COPY) {
      //          /* Copy payload to space following the ..msg_t */
      //          rkm->rkm_payload = p;
      //          memcpy(rkm->rkm_payload, payload, len);
      //          p += len;
      //  } else {
      //          /* Just point to the provided payload. */
      //          rkm->rkm_payload = payload;
      //
      // Further git-grepping for rkm_payload didn't show anything fishy.
      const_cast<char*>(reinterpret_cast<const char*>(bytes.data())),
      // The payload size.
      bytes.size(),
      // Message key.
      (key.empty() ? nullptr : key.data()), key.size(),
      // Timestamp (ms since UTC epoch; 0 = current time).
      ms,
      // Per-message opaque value passed to delivery report.
      nullptr);
    switch (result) {
      default:
        return caf::make_error(ec::unspecified, err2str(result));
      case RdKafka::ERR_NO_ERROR: {
        producer_->poll(0);
        return {};
      }
      case RdKafka::ERR__QUEUE_FULL: {
        // The internal queue represents both messages to be sent and messages
        // that have been sent or failed, awaiting their delivery report
        // callback to be called.
        //
        // The internal queue is limited by the configuration property
        // queue.buffering.max.messages and queue.buffering.max.kbytes
        auto ms = 1000;
        TENZIR_WARN("queue full, retrying in {}ms", ms);
        producer_->poll(ms);
        break;
      }
    }
  }
  __builtin_unreachable();
}

auto producer::poll(std::chrono::milliseconds timeout) -> int {
  auto ms = detail::narrow_cast<int>(timeout.count());
  return producer_->poll(ms);
}

auto producer::flush(std::chrono::milliseconds timeout) -> caf::error {
  auto ms = detail::narrow_cast<int>(timeout.count());
  auto result = producer_->flush(ms);
  switch (result) {
    case RdKafka::ERR_NO_ERROR:
      break;
    case RdKafka::ERR__TIMED_OUT:
      return ec::timeout;
    default:
      return caf::make_error(ec::unspecified,
                             fmt::format("failed to flush message: {}",
                                         RdKafka::err2str(result)));
  }
  return {};
}

auto producer::queue_size() const -> size_t {
  return static_cast<size_t>(producer_->outq_len());
}

} // namespace tenzir::plugins::kafka
