//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/expression_visitors.hpp"

#include "tenzir/fwd.hpp"

#include "tenzir/concept/parseable/tenzir/data.hpp"
#include "tenzir/concept/parseable/to.hpp"
#include "tenzir/concept/printable/tenzir/data.hpp"
#include "tenzir/concept/printable/tenzir/operator.hpp"
#include "tenzir/concept/printable/to_string.hpp"
#include "tenzir/data.hpp"
#include "tenzir/detail/assert.hpp"
#include "tenzir/ids.hpp"
#include "tenzir/logger.hpp"
#include "tenzir/table_slice.hpp"
#include "tenzir/type.hpp"
#include "tenzir/view.hpp"

#include <algorithm>
#include <regex>

namespace tenzir {

expression meta_pruner::operator()(caf::none_t) const {
  return expression{};
}

expression meta_pruner::operator()(const conjunction& c) const {
  conjunction pruned;
  for (const auto& op : c) {
    auto x = match(op, *this);
    if (x != expression{}) {
      pruned.push_back(std::move(x));
    }
  }
  if (pruned.empty()) {
    return expression{};
  }
  if (pruned.size() == 1) {
    return pruned.front();
  }
  return pruned;
}

expression meta_pruner::operator()(const disjunction& d) const {
  disjunction pruned;
  for (const auto& op : d) {
    auto x = match(op, *this);
    if (x != expression{}) {
      pruned.push_back(std::move(x));
    }
  }
  if (pruned.empty()) {
    return expression{};
  }
  if (pruned.size() == 1) {
    return pruned.front();
  }
  return pruned;
}

expression meta_pruner::operator()(const negation& n) const {
  auto x = match(n.expr(), *this);
  if (x == expression{}) {
    return x;
  }
  return negation{x};
}

expression meta_pruner::operator()(const predicate& p) const {
  if (is<meta_extractor>(p.lhs) || is<meta_extractor>(p.rhs)) {
    return expression{};
  }
  return {p};
}

expression hoister::operator()(caf::none_t) const {
  return expression{};
}

expression hoister::operator()(const conjunction& c) const {
  conjunction hoisted;
  for (auto& op : c) {
    if (auto inner = try_as<conjunction>(&op)) {
      for (auto& inner_op : *inner) {
        hoisted.push_back(match(inner_op, *this));
      }
    } else {
      hoisted.push_back(match(op, *this));
    }
  }
  return hoisted.size() == 1 ? hoisted[0] : hoisted;
}

expression hoister::operator()(const disjunction& d) const {
  disjunction hoisted;
  for (auto& op : d) {
    if (auto inner = try_as<disjunction>(&op)) {
      for (auto& inner_op : *inner) {
        hoisted.push_back(match(inner_op, *this));
      }
    } else {
      hoisted.push_back(match(op, *this));
    }
  }
  return hoisted.size() == 1 ? hoisted[0] : hoisted;
}

expression hoister::operator()(const negation& n) const {
  return {negation{match(n.expr(), *this)}};
}

expression hoister::operator()(const predicate& p) const {
  return {p};
}

expression aligner::operator()(caf::none_t) const {
  return caf::none;
}

expression aligner::operator()(const conjunction& c) const {
  conjunction result;
  for (auto& op : c) {
    result.push_back(match(op, *this));
  }
  return result;
}

expression aligner::operator()(const disjunction& d) const {
  disjunction result;
  for (auto& op : d) {
    result.push_back(match(op, *this));
  }
  return result;
}

expression aligner::operator()(const negation& n) const {
  return {negation{match(n.expr(), *this)}};
}

expression aligner::operator()(const predicate& p) const {
  auto is_extractor = [](auto& operand) {
    return !is<data>(operand);
  };
  // Already aligned if LHS is an extractor or no extractor present.
  if (is_extractor(p.lhs) || !is_extractor(p.rhs)) {
    return p;
  }
  return predicate{p.rhs, flip(p.op), p.lhs};
}

denegator::denegator(bool negate) : negate_{negate} {
}

expression denegator::operator()(caf::none_t) const {
  return caf::none;
}

expression denegator::operator()(const conjunction& c) const {
  auto add = [&](auto x) -> expression {
    for (auto& op : c) {
      x.push_back(match(op, *this));
    }
    return x;
  };
  return negate_ ? add(disjunction{}) : add(conjunction{});
}

expression denegator::operator()(const disjunction& d) const {
  auto add = [&](auto x) -> expression {
    for (auto& op : d) {
      x.push_back(match(op, *this));
    }
    return x;
  };
  return negate_ ? add(conjunction{}) : add(disjunction{});
}

expression denegator::operator()(const negation& n) const {
  // Step through double negations.
  if (auto inner = try_as<negation>(&n.expr())) {
    return match(inner->expr(), *this);
  }
  // Apply De Morgan from here downward.
  return match(n.expr(), denegator{!negate_});
}

expression denegator::operator()(const predicate& p) const {
  return predicate{p.lhs, negate_ ? negate(p.op) : p.op, p.rhs};
}

expression deduplicator::operator()(caf::none_t) const {
  return caf::none;
}

expression deduplicator::operator()(const conjunction& c) const {
  conjunction result;
  for (auto& op : c) {
    auto p = match(op, *this);
    if (std::count(result.begin(), result.end(), p) == 0) {
      result.push_back(p);
    }
  }
  return result;
}

expression deduplicator::operator()(const disjunction& d) const {
  disjunction result;
  for (auto& op : d) {
    auto p = match(op, *this);
    if (std::count(result.begin(), result.end(), p) == 0) {
      result.push_back(p);
    }
  }
  return result;
}

expression deduplicator::operator()(const negation& n) const {
  return match(n.expr(), *this);
}

expression deduplicator::operator()(const predicate& p) const {
  return p;
}

namespace {

template <class Ts, class Us>
auto inplace_union(Ts& xs, const Us& ys) {
  auto mid = xs.size();
  std::copy(ys.begin(), ys.end(), std::back_inserter(xs));
  std::inplace_merge(xs.begin(), xs.begin() + mid, xs.end());
  xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
}

} // namespace

std::vector<predicate> predicatizer::operator()(caf::none_t) const {
  return {};
}

std::vector<predicate> predicatizer::operator()(const conjunction& con) const {
  std::vector<predicate> result;
  for (auto& op : con) {
    auto ps = match(op, *this);
    inplace_union(result, ps);
  }
  return result;
}

std::vector<predicate> predicatizer::operator()(const disjunction& dis) const {
  std::vector<predicate> result;
  for (auto& op : dis) {
    auto ps = match(op, *this);
    inplace_union(result, ps);
  }
  return result;
}

std::vector<predicate> predicatizer::operator()(const negation& n) const {
  return match(n.expr(), *this);
}

std::vector<predicate> predicatizer::operator()(const predicate& pred) const {
  return {pred};
}

caf::expected<void> validator::operator()(caf::none_t) {
  return caf::make_error(ec::syntax_error, "null expression is invalid");
}

caf::expected<void> validator::operator()(const conjunction& c) {
  for (auto& op : c) {
    auto m = match(op, *this);
    if (!m) {
      return m;
    }
  }
  return {};
}

caf::expected<void> validator::operator()(const disjunction& d) {
  for (auto& op : d) {
    auto m = match(op, *this);
    if (!m) {
      return m;
    }
  }
  return {};
}

caf::expected<void> validator::operator()(const negation& n) {
  return match(n.expr(), *this);
}

caf::expected<void> validator::operator()(const predicate& p) {
  op_ = p.op;
  return match(std::tie(p.lhs, p.rhs), *this);
}

caf::expected<void>
validator::operator()(const meta_extractor& ex, const data& d) {
  if (ex.kind == meta_extractor::schema
      && !(is<std::string>(d) || is<pattern>(d))) {
    return caf::make_error(ec::syntax_error,
                           "schema meta extractor requires string or pattern "
                           "operand",
                           "#schema", op_, d);
  }
  if (ex.kind == meta_extractor::schema_id
      && !(is<std::string>(d) || is<pattern>(d))) {
    return caf::make_error(ec::syntax_error,
                           "schema_id meta extractor requires string or "
                           "pattern operand",
                           "#schema_id", op_, d);
  }
  if (ex.kind == meta_extractor::import_time) {
    if (!is<time>(d)
        || !(op_ == relational_operator::less
             || op_ == relational_operator::less_equal
             || op_ == relational_operator::greater
             || op_ == relational_operator::greater_equal)) {
      return caf::make_error(ec::syntax_error,
                             fmt::format("import_time attribute extractor only "
                                         "supports time comparisons "
                                         "#import_time {} {}",
                                         op_, d));
    }
  }
  return {};
}

caf::expected<void>
validator::operator()(const type_extractor& ex, const data& d) {
  // References to aliases can't be checked here because the expression parser
  // can't possible know about them. We defer the check to the type resolver.
  if (!ex.type) {
    return {};
  }
  if (!compatible(ex.type, op_, d)) {
    return caf::make_error(
      ec::syntax_error, "type extractor type check failure", ex.type, op_, d);
  }
  return {};
}

caf::expected<void> validator::operator()(const field_extractor&, const data&) {
  // Validity of a field extractor requires a specific schema, which we don't
  // have in this context.
  return {};
}

type_resolver::type_resolver(const type& schema)
  : schema_{as<record_type>(schema)}, schema_name_{schema.name()} {
  // nop
}

caf::expected<expression> type_resolver::operator()(caf::none_t) {
  return expression{};
}

caf::expected<expression> type_resolver::operator()(const conjunction& c) {
  conjunction result;
  for (auto& op : c) {
    auto r = match(op, *this);
    if (!r) {
      return r;
    } else if (is<caf::none_t>(*r)) {
      return expression{};
    } else {
      result.push_back(std::move(*r));
    }
  }
  if (result.empty()) {
    return expression{};
  }
  if (result.size() == 1) {
    return {std::move(result[0])};
  } else {
    return {std::move(result)};
  }
}

caf::expected<expression> type_resolver::operator()(const disjunction& d) {
  disjunction result;
  for (auto& op : d) {
    auto r = match(op, *this);
    if (!r) {
      return r;
    } else if (!is<caf::none_t>(*r)) {
      result.push_back(std::move(*r));
    }
  }
  if (result.empty()) {
    return expression{};
  }
  if (result.size() == 1) {
    return {std::move(result[0])};
  } else {
    return {std::move(result)};
  }
}

caf::expected<expression> type_resolver::operator()(const negation& n) {
  auto r = match(n.expr(), *this);
  if (!r) {
    return r;
  } else if (!is<caf::none_t>(*r)) {
    return {negation{std::move(*r)}};
  } else {
    return expression{};
  }
}

caf::expected<expression> type_resolver::operator()(const predicate& p) {
  op_ = p.op;
  return match(std::tie(p.lhs, p.rhs), *this);
}

caf::expected<expression>
type_resolver::operator()(const meta_extractor& ex, const data& d) {
  // We're leaving all attributes alone, because both #schema and #import_time
  // operate at a different granularity.
  return predicate{ex, op_, d};
}

caf::expected<expression>
type_resolver::operator()(const type_extractor& ex, const data& d) {
  if (!ex.type) {
    auto matches = [&](const type& t) {
      return t.name() == ex.type.name() and compatible(t, op_, d);
    };
    return resolve_extractor(matches, d);
  }
  auto is_congruent = [&](const type& t) {
    return congruent(t, ex.type);
  };
  return resolve_extractor(is_congruent, d);
}

caf::expected<expression>
type_resolver::operator()(const data& d, const type_extractor& ex) {
  return (*this)(ex, d);
}

caf::expected<expression>
type_resolver::operator()(const field_extractor& ex, const data& d) {
  std::vector<expression> connective;
  // First, interpret the field as a suffix of a record field name.
  auto suffixes = schema_.resolve_key_suffix(ex.field, schema_name_);
  for (auto&& offset : suffixes) {
    const auto f = schema_.field(offset);
    if (not compatible(f.type, op_, d)) {
      continue;
    }
    auto x = data_extractor{schema_, offset};
    connective.emplace_back(predicate{std::move(x), op_, d});
  }
  if (connective.empty()) {
    return expression{}; // did not resolve
  }
  if (connective.size() == 1) {
    return {std::move(connective[0])};
  }
  if (is_negated(op_)) {
    return {conjunction{std::move(connective)}};
  }
  return {disjunction{std::move(connective)}};
}

caf::expected<expression>
type_resolver::operator()(const data& d, const field_extractor& ex) {
  return (*this)(ex, d);
}

matcher::matcher(const type& t) : type_{t} {
  // nop
}

bool matcher::operator()(caf::none_t) {
  return false;
}

bool matcher::operator()(const conjunction& c) {
  for (auto& op : c) {
    if (!match(op, *this)) {
      return false;
    }
  }
  return true;
}

bool matcher::operator()(const disjunction& d) {
  for (auto& op : d) {
    if (match(op, *this)) {
      return true;
    }
  }
  return false;
}

bool matcher::operator()(const negation& n) {
  return match(n.expr(), *this);
}

bool matcher::operator()(const predicate& p) {
  op_ = p.op;
  return match(std::tie(p.lhs, p.rhs), *this);
}

bool matcher::operator()(const meta_extractor& e, const data& d) {
  if (e.kind == meta_extractor::schema) {
    TENZIR_ASSERT(is<std::string>(d));
    // TODO: It's kind of non-sensical that evaluate operates on data rather
    // than data_view, forcing us to copy the type's name here.
    return evaluate(d, op_, std::string{type_.name()});
  }
  if (e.kind == meta_extractor::schema_id) {
    TENZIR_ASSERT(is<std::string>(d));
    return evaluate(d, op_, type_.make_fingerprint());
  }
  return false;
}

bool matcher::operator()(const data_extractor&, const data&) {
  // If we encounter a data_extractor, it must have been created through a
  // previous invocation of a type_resolver visitation. The presence of
  // data_extractor indicates that the expression matches.
  return true;
}

} // namespace tenzir
