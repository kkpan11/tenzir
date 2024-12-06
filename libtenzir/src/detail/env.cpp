//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/detail/env.hpp"

#include "tenzir/detail/posix.hpp"
#include "tenzir/error.hpp"

#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string_view>

extern char** environ;

namespace tenzir::detail {

namespace {

// A mutex for locking calls to functions that mutate `environ`. Global to this
// translation unit.
auto env_mutex = std::mutex{};

} // namespace

std::optional<std::string> getenv(std::string_view var) {
  auto lock = std::scoped_lock{env_mutex};
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (const char* result = ::getenv(var.data()))
    return std::string{result};
  return {};
}

caf::error setenv(std::string_view key, std::string_view value, int overwrite) {
  auto lock = std::scoped_lock{env_mutex};
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (::setenv(key.data(), value.data(), overwrite) == 0)
    return {};
  return caf::make_error( //
    ec::system_error,
    fmt::format("failed in setenv(3): {}", detail::describe_errno()));
}

caf::error unsetenv(std::string_view var) {
  auto lock = std::scoped_lock{env_mutex};
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (::unsetenv(var.data()) == 0)
    return {};
  return caf::make_error( //
    ec::system_error,
    fmt::format("failed in unsetenv(3): {}", detail::describe_errno()));
}

generator<std::pair<std::string_view, std::string_view>> environment() {
  // Envrionment variables come as "key=value" pair strings.
  auto lock = std::scoped_lock{env_mutex};
  for (auto env = environ; *env != nullptr; ++env) {
    auto str = std::string_view{*env};
    auto i = str.find('=');
    if (i != std::string::npos) {
      auto key = str.substr(0, i);
      auto value = str.substr(i + 1);
      co_yield std::pair{key, value};
    }
  }
}

} // namespace tenzir::detail
