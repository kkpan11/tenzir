//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "tenzir/fwd.hpp"

#include "tenzir/detail/stack_vector.hpp"
#include "tenzir/hash/hash.hpp"

#include <compare>
#include <cstddef>
#include <span>

namespace tenzir {

/// A sequence of indexes to recursively access a type or value.
struct offset : detail::stack_vector<size_t, 64> {
  using super = detail::stack_vector<size_t, 64>;
  using super::super;

  template <class Inspector>
  friend auto inspect(Inspector& f, offset& x) {
    return f.object(x)
      .pretty_name("tenzir.offset")
      .fields(f.field("value", static_cast<super&>(x)));
  }

  friend auto operator<=>(const offset& lhs, const offset& rhs) noexcept
    -> std::strong_ordering;

  friend auto operator==(const offset& lhs, const offset& rhs) noexcept -> bool;

  /// Access a nested array in a table slice, record batch, or struct array. The
  /// offset is assumed to be valid.
  ///
  /// NOTE: This function solely exists because the logically equivalent
  /// arrow::FieldPath::Get(...) is fundamentally broken for arrays with a
  /// non-zero offset (i.e., after calling Array::Slice(...) on them. The Arrow
  /// function re-assembles the resulting array from the underlying array data,
  /// discarding the offset and the length, and may as such return arrays longer
  /// than the input data.
  auto get(const table_slice& slice) const noexcept
    -> std::pair<type, std::shared_ptr<arrow::Array>>;
  auto get(const arrow::RecordBatch& batch) const noexcept
    -> std::shared_ptr<arrow::Array>;
  auto get(const arrow::StructArray& struct_array) const noexcept
    -> std::shared_ptr<arrow::Array>;
};

} // namespace tenzir

namespace std {

template <>
struct hash<tenzir::offset> {
  size_t operator()(const tenzir::offset& x) const noexcept {
    return tenzir::hash(x);
  }
};

} // namespace std
