//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/flow.hpp"

#include "tenzir/concept/parseable/tenzir/ip.hpp"
#include "tenzir/detail/assert.hpp"

#include <optional>
#include <string_view>

namespace tenzir {

std::optional<flow>
make_flow(std::string_view src_addr, std::string_view dst_addr,
          uint16_t src_port, uint16_t dst_port, port_type protocol) {
  using parsers::ip;
  flow result;
  if (!ip(src_addr, result.src_addr) || !ip(dst_addr, result.dst_addr))
    return {};
  result.src_port = port{src_port, protocol};
  result.dst_port = port{dst_port, protocol};
  return result;
}

bool operator==(const flow& x, const flow& y) {
  return x.src_addr == y.src_addr && x.dst_addr == y.dst_addr
         && x.src_port == y.src_port && x.dst_port == y.dst_port;
}

port_type protocol(const flow& x) {
  TENZIR_ASSERT_EXPENSIVE(x.src_port.type() == x.dst_port.type());
  return x.src_port.type();
}

} // namespace tenzir
