//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "tenzir/concepts.hpp"
#include "tenzir/detail/type_traits.hpp"
#include "tenzir/hash/concepts.hpp"
#include "tenzir/hash/default_hash.hpp"
#include "tenzir/hash/hash_append.hpp"
#include "tenzir/hash/uniquely_hashable.hpp"
#include "tenzir/hash/uniquely_represented.hpp"

#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

namespace tenzir {
namespace detail {

/// Proxy type to dispatch a hash operation to one or more calls in the
/// underlying hash algorithm.
template <hash_algorithm HashAlgorithm = default_hash, class... Seeds>
struct hash_algorithm_proxy {
  template <class... Ts>
  explicit hash_algorithm_proxy(Ts&&... xs) noexcept
    : seeds{std::forward<Ts>(xs)...} {
  }

  template <class T>
    requires(uniquely_hashable<T, HashAlgorithm>)
  static auto sequentialize(const T& x) noexcept {
    if constexpr (uniquely_represented<T>) {
      const auto* ptr = reinterpret_cast<const std::byte*>(std::addressof(x));
      return std::span<const std::byte, sizeof(T)>{ptr, sizeof(x)};
    } else if constexpr (concepts::fixed_byte_sequence<T>) {
      return as_bytes(x);
    } else {
      static_assert(always_false_v<T>, "T is not sequentializable");
    }
  }

  template <class T>
  auto operator()(const T& x) const noexcept ->
    typename HashAlgorithm::result_type {
    if constexpr (uniquely_hashable<T, HashAlgorithm>) {
      auto bytes = sequentialize(x);
      if constexpr (oneshot_hash<HashAlgorithm>) {
        auto make = [](const auto&... xs) noexcept {
          return HashAlgorithm::make(xs...);
        };
        auto args = std::tuple_cat(std::tuple(bytes), seeds);
        return std::apply(make, args);
      } else {
        auto h = std::make_from_tuple<HashAlgorithm>(seeds);
        h.add(bytes);
        return std::move(h).finish();
      }
    } else if constexpr (incremental_hash<HashAlgorithm>) {
      auto h = std::make_from_tuple<HashAlgorithm>(seeds);
      hash_append(h, x);
      return std::move(h).finish();
    } else {
      static_assert(always_false_v<T>, "T is not hashable");
    }
  }

  template <class... Ts>
  auto operator()(const Ts&... xs) const noexcept ->
    typename HashAlgorithm::result_type {
    auto h = std::make_from_tuple<HashAlgorithm>(seeds);
    hash_append(h, xs...);
    return std::move(h).finish();
  }

  std::tuple<Seeds...> seeds;
};

} // namespace detail

/// Generic function to compute a hash diegst over a hashable type. The
/// implementation chooses the fastest possible way to compute the hash.
/// @tparam The hash algorithm to use for hashing.
/// @param x The value to hash.
/// @returns A hash digest of *x* using `HashAlgorithm`.
/// @relates seeded_hash
template <hash_algorithm HashAlgorithm = default_hash, class T, class... Ts>
[[nodiscard]] auto hash(const T& x, const Ts&... xs) noexcept {
  return detail::hash_algorithm_proxy<HashAlgorithm>{}(x, xs...);
}

/// Generic API to compute a seeded hash. Users can invoke this proxy object
/// similar to compute an unseeded hash: `seeded_hash{s}(x)` for a seed `s` and
/// hashable object `x`. The number of seeds is variable and depends on what
/// the underlying hash algorithm exposes. Most algorithms offer APIs where
/// users can provide one seed.
/// @relates hash
template <hash_algorithm HashAlgorithm = default_hash,
          class Seed = typename HashAlgorithm::seed_type, class... Seeds>
using seeded_hash = detail::hash_algorithm_proxy<HashAlgorithm, Seed, Seeds...>;

} // namespace tenzir
