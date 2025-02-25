//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2019 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "tenzir/concept/printable/core/printer.hpp"

#include <string>
#include <string_view>

namespace tenzir {

template <class Escaper>
struct escape_printer : printer_base<escape_printer<Escaper>> {
  using attribute = std::string_view;

  constexpr explicit escape_printer(Escaper f) : escaper{f} {
    // nop
  }

  template <class Iterator>
  bool print(Iterator& out, std::string_view str) const {
    auto f = str.begin();
    auto l = str.end();
    while (f != l)
      escaper(f, out);
    return true;
  }

  Escaper escaper;
};

namespace printers {

template <class Escaper>
constexpr auto escape(Escaper escaper) {
  return escape_printer<Escaper>{escaper};
}

} // namespace printers
} // namespace tenzir
