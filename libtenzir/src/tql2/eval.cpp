//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2024 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/tql2/eval.hpp"

#include "tenzir/detail/assert.hpp"
#include "tenzir/diagnostics.hpp"
#include "tenzir/tql2/ast.hpp"
#include "tenzir/tql2/eval_impl.hpp"
#include "tenzir/try.hpp"

/// TODO:
/// - Reduce series expansion. For example, `src_ip in [1.2.3.4, 1.2.3.5]`
///   currently creates `length` copies of the list.
/// - Optimize expressions, e.g., constant folding, compute offsets.
/// - Short circuiting, active rows.
/// - Stricter behavior for const-eval, or same behavior? For example, overflow.
/// - Modes for "must be constant", "prefer constant", "prefer runtime", "must
///   be runtime".
/// - Integrate type checker?

namespace tenzir {

auto resolve(const ast::simple_selector& sel, const table_slice& slice)
  -> variant<series, resolve_error> {
  TRY(auto offset, resolve(sel, slice.schema()));
  auto [ty, array] = offset.get(slice);
  return series{ty, array};
}

auto resolve(const ast::simple_selector& sel, type ty)
  -> variant<offset, resolve_error> {
  // TODO: Write this properly.
  auto sel_index = size_t{0};
  auto result = offset{};
  auto&& path = sel.path();
  result.reserve(path.size());
  while (sel_index < path.size()) {
    auto rty = try_as<record_type>(ty);
    if (not rty) {
      return resolve_error{path[sel_index],
                           resolve_error::field_of_non_record{ty}};
    }
    auto found = false;
    auto field_index = size_t{0};
    for (auto&& field : rty->fields()) {
      if (field.name == path[sel_index].name) {
        ty = field.type;
        found = true;
        sel_index += 1;
        break;
      }
      ++field_index;
    }
    if (not found) {
      return resolve_error{path[sel_index], resolve_error::field_not_found{}};
    }
    result.push_back(field_index);
  }
  return result;
}

auto eval(const ast::expression& expr, const table_slice& input,
          diagnostic_handler& dh) -> multi_series {
  // TODO: Do not create a new session here.
  auto sp = session_provider::make(dh);
  auto result = evaluator{&input, sp.as_session()}.eval(expr);
  TENZIR_ASSERT(result.length() == detail::narrow<int64_t>(input.rows()));
  return result;
}

auto eval(const ast::simple_selector& expr, const table_slice& input,
          diagnostic_handler& dh) -> series {
  auto result = eval(expr.inner(), input, dh);
  TENZIR_ASSERT(result.length() == detail::narrow<int64_t>(input.rows()));
  TENZIR_ASSERT(result.parts().size() == 1);
  return std::move(result.parts()[0]);
}

auto const_eval(const ast::expression& expr, diagnostic_handler& dh)
  -> failure_or<data> {
  // TODO: Do not create a new session here.
  try {
    auto sp = session_provider::make(dh);
    auto result = evaluator{nullptr, sp.as_session()}.eval(expr);
    TENZIR_ASSERT(result.length() == 1);
    TENZIR_ASSERT(result.parts().size() == 1);
    auto& part = result.part(0);
    return materialize(value_at(part.type, *part.array, 0));
  } catch (failure fail) {
    return fail;
  }
}

auto try_const_eval(const ast::expression& expr, diagnostic_handler& dh)
  -> std::optional<data> {
  auto const_dh = collecting_diagnostic_handler{};
  auto const_sp = session_provider::make(const_dh);
  if (auto result = const_eval(expr, const_dh)) {
    std::move(const_dh).forward_to(dh);
    return std::move(*result);
  }
  return {};
}

} // namespace tenzir
