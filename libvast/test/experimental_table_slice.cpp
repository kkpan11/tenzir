//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2021 The VAST Contributors
// SPDX-License-Identifier: BSD-3-Clause

#define SUITE experimental_table_slice

#include "vast/arrow_extension_types.hpp"
#include "vast/arrow_table_slice.hpp"
#include "vast/concept/parseable/to.hpp"
#include "vast/concept/parseable/vast/subnet.hpp"
#include "vast/config.hpp"
#include "vast/detail/narrow.hpp"
#include "vast/experimental_table_slice_builder.hpp"
#include "vast/test/fixtures/table_slices.hpp"
#include "vast/test/test.hpp"
#include "vast/type.hpp"

#include <arrow/record_batch.h>
#include <arrow/type_fwd.h>
#include <caf/test/dsl.hpp>

#include <utility>

using namespace vast;
using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

template <class... Ts>
auto make_slice(const record_type& layout, Ts&&... xs) {
  auto builder = experimental_table_slice_builder::make(type{"stub", layout});
  auto ok = builder->add(std::forward<Ts>(xs)...);
  if (!ok)
    FAIL("builder failed to add given values");
  auto slice = builder->finish();
  if (slice.encoding() == table_slice_encoding::none)
    FAIL("builder failed to produce a table slice");
  return slice;
}

template <class T, class... Ts>
auto make_slice(const record_type& layout, std::vector<T> x0,
                std::vector<Ts>... xs) {
  auto builder = experimental_table_slice_builder::make(type{"rec", layout});
  for (size_t i = 0; i < x0.size(); ++i) {
    CHECK(builder->add(x0.at(i)));
    CHECK(builder->add(xs.at(i)...));
  }
  return builder->finish();
}

template <concrete_type T, class V>
auto check_column(const table_slice& slice, int c, const T& t,
                  const std::vector<V>& ref) {
  for (size_t r = 0; r < ref.size(); ++r)
    CHECK_VARIANT_EQUAL(slice.at(r, c, t), make_view(ref[r]));
}

count operator"" _c(unsigned long long int x) {
  return static_cast<count>(x);
}

template <concrete_type VastType, class... Ts>
auto make_single_column_slice(const VastType& t, const Ts&... xs) {
  record_type layout{{"foo", t}};
  return make_slice(layout, xs...);
}

table_slice roundtrip(table_slice slice) {
  factory<table_slice_builder>::add<experimental_table_slice_builder>(
    table_slice_encoding::experimental);
  table_slice slice_copy;
  std::vector<char> buf;
  caf::binary_serializer sink{nullptr, buf};
  CHECK_EQUAL(inspect(sink, slice), caf::none);
  CHECK_EQUAL(detail::legacy_deserialize(buf, slice_copy), true);
  return slice_copy;
}

void record_batch_roundtrip(const table_slice& slice) {
  const auto copy
    = experimental_table_slice_builder::create(to_record_batch(slice));
  CHECK_EQUAL(slice, copy);
}

enumeration operator"" _e(unsigned long long int x) {
  return static_cast<enumeration>(x);
}

integer operator"" _i(unsigned long long int x) {
  return integer{detail::narrow<integer::value_type>(x)};
}

#define CHECK_OK(expression)                                                   \
  if (!(expression).ok())                                                      \
    FAIL("!! " #expression);

} // namespace

TEST(nested multi - column roundtrip) {
  auto t = record_type{
    {"f1", type{string_type{}, {{"key", "value"}}}},
    {"f2", type{"alt_name", count_type{}}},
    {
      "f3_rec",
      type{
        "nested",
        record_type{
          {"f3.1", type{"rgx", pattern_type{}, {{"index", "none"}}}},
          {"f3.2", integer_type{}},
        },
        {{"attr"}, {"other_attr", "val"}},
      },
    },
  };
  auto f1s = std::vector<std::string>{"n1", "n2", "n3", "n4"};
  auto f2s = std::vector<count>{1_c, 2_c, 3_c, 4_c};
  auto f3s = std::vector<pattern>{pattern("p1"), pattern("p2"), pattern("p3"),
                                  pattern("p4")};
  auto f4s = std::vector<integer>{8_i, 7_i, 6_i, 5_i};
  auto slice = make_slice(t, f1s, f2s, f3s, f4s);
  check_column(slice, 0, string_type{}, f1s);
  check_column(slice, 1, count_type{}, f2s);
  check_column(slice, 2, pattern_type{}, f3s);
  check_column(slice, 3, integer_type{}, f4s);
  record_batch_roundtrip(slice);
}

TEST(single column - equality) {
  auto t = count_type{};
  auto slice1 = make_single_column_slice(t, 0_c, 1_c, caf::none, 3_c);
  auto slice2 = make_single_column_slice(t, 0_c, 1_c, caf::none, 3_c);
  CHECK_VARIANT_EQUAL(slice1.at(0, 0, t), slice2.at(0, 0, t));
  CHECK_VARIANT_EQUAL(slice1.at(1, 0, t), slice2.at(1, 0, t));
  CHECK_VARIANT_EQUAL(slice1.at(2, 0, t), slice2.at(2, 0, t));
  CHECK_VARIANT_EQUAL(slice1.at(3, 0, t), slice2.at(3, 0, t));
  CHECK_EQUAL(slice1, slice1);
  CHECK_EQUAL(slice1, slice2);
  CHECK_EQUAL(slice2, slice1);
  CHECK_EQUAL(slice2, slice2);
}

TEST(single column - count) {
  auto t = count_type{};
  auto slice = make_single_column_slice(t, 0_c, 1_c, caf::none, 3_c);
  REQUIRE_EQUAL(slice.rows(), 4u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), 0_c);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), 1_c);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(3, 0, t), 3_c);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - enumeration) {
  auto t = enumeration_type{{"foo"}, {"bar"}, {"baz"}};
  auto slice = make_single_column_slice(t, 2_e, 1_e, 0_e, 2_e, caf::none);
  REQUIRE_EQUAL(slice.rows(), 5u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), 2_e);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), 1_e);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), 0_e);
  CHECK_VARIANT_EQUAL(slice.at(3, 0, t), 2_e);
  CHECK_VARIANT_EQUAL(slice.at(4, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - enum2) {
  auto t = enumeration_type{{"a"}, {"b"}, {"c"}, {"d"}};
  auto slice = make_single_column_slice(t, 0_e, 1_e, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), 0_e);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), 1_e);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - integer) {
  auto t = integer_type{};
  auto slice = make_single_column_slice(t, caf::none, 1_i, 2_i);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), 1_i);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), 2_i);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - boolean) {
  auto t = bool_type{};
  auto slice = make_single_column_slice(t, false, caf::none, true);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), false);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), true);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - real) {
  auto t = real_type{};
  auto slice = make_single_column_slice(t, 1.23, 3.21, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), 1.23);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), 3.21);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - string) {
  auto t = string_type{};
  auto slice = make_single_column_slice(t, "a"sv, caf::none, "c"sv);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), "a"sv);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), "c"sv);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - pattern) {
  auto t = pattern_type{};
  auto p1 = pattern("foo.ar");
  auto p2 = pattern("hello* world");
  auto slice = make_single_column_slice(t, p1, p2, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), make_view(p1));
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), make_view(p2));
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - time) {
  using ts = vast::time;
  auto epoch = ts{duration{0}};
  auto t = time_type{};
  auto slice = make_single_column_slice(t, epoch, caf::none, epoch + 48h);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), epoch);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), epoch + 48h);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - duration) {
  auto h0 = duration{0};
  auto h12 = h0 + 12h;
  auto t = duration_type{};
  auto slice = make_single_column_slice(t, h0, h12, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), h0);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), h12);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - address) {
  using vast::address;
  using vast::to;
  auto t = address_type{};
  auto a1 = unbox(to<address>("172.16.7.1"));
  auto a2 = unbox(to<address>("ff01:db8::202:b3ff:fe1e:8329"));
  auto a3 = unbox(to<address>("2001:db8::"));
  auto slice = make_single_column_slice(t, caf::none, a1, a2, a3);
  REQUIRE_EQUAL(slice.rows(), 4u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), a1);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), a2);
  CHECK_VARIANT_EQUAL(slice.at(3, 0, t), a3);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - subnet) {
  using vast::subnet;
  using vast::to;
  auto t = subnet_type{};
  auto s1 = unbox(to<subnet>("172.16.7.0/8"));
  auto s2 = unbox(to<subnet>("172.16.0.0/16"));
  auto s3 = unbox(to<subnet>("172.0.0.0/24"));
  auto slice = make_single_column_slice(t, s1, s2, s3, caf::none);
  REQUIRE_EQUAL(slice.rows(), 4u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), s1);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), s2);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), s3);
  CHECK_VARIANT_EQUAL(slice.at(3, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - list of integers) {
  auto t = list_type{integer_type{}};
  record_type layout{{"values", t}};
  list list1{1_i, 2_i, 3_i};
  list list2{10_i, 20_i};
  auto slice = make_slice(layout, list1, caf::none, list2);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), make_view(list1));
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), make_view(list2));
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(list of structs) {
  auto unbox_ref = [](auto&& x) -> decltype(auto) {
    if (!x)
      FAIL("x == nullptr");
    return *std::forward<decltype(x)>(x);
  };
  auto layout = record_type{
    {
      "foo",
      list_type{
        record_type{
          {"bar", count_type{}},
          {"baz", count_type{}},
        },
      },
    },
  };
  auto foo1 = list{
    record{
      {"bar", 1_c},
      {"baz", 2_c},
    },
    record{
      {"bar", 3_c},
      {"baz", caf::none},
    },
  };
  auto foo2 = caf::none;
  auto foo3 = list{
    record{
      {"bar", caf::none},
      {"baz", 6_c},
    },
  };
  auto foo4 = list{
    record{
      {"bar", caf::none},
      {"baz", caf::none},
    },
  };
  auto slice = make_slice(layout, foo1, foo2, foo3, foo4);
  auto batch = to_record_batch(slice);
  const auto& list_col
    = unbox_ref(caf::get_if<arrow::ListArray>(batch->column(0).get()));
  REQUIRE_EQUAL(list_col.length(), 4u);
  {
    MESSAGE("access foo (across boundaries)");
    const auto& foo_col
      = unbox_ref(caf::get_if<arrow::StructArray>(list_col.values().get()));
    const auto& bar_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo_col.field(0).get()));
    const auto& baz_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo_col.field(1).get()));
    REQUIRE_EQUAL(bar_col.length(), 4u);
    CHECK_EQUAL(bar_col.Value(0), 1u);
    CHECK_EQUAL(bar_col.Value(1), 3u);
    CHECK(bar_col.IsNull(2));
    CHECK(bar_col.IsNull(3));
    REQUIRE_EQUAL(baz_col.length(), 4u);
    CHECK_EQUAL(baz_col.Value(0), 2u);
    CHECK(baz_col.IsNull(1));
    CHECK_EQUAL(baz_col.Value(2), 6u);
    CHECK(baz_col.IsNull(3));
  }
  {
    MESSAGE("access foo1");
    REQUIRE(!list_col.IsNull(0));
    auto foo1_col_slice = list_col.value_slice(0);
    const auto& foo1_col
      = unbox_ref(caf::get_if<arrow::StructArray>(foo1_col_slice.get()));
    const auto& bar1_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo1_col.field(0).get()));
    const auto& baz1_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo1_col.field(1).get()));
    REQUIRE_EQUAL(bar1_col.length(), 2u);
    CHECK_EQUAL(bar1_col.Value(0), 1u);
    CHECK_EQUAL(bar1_col.Value(1), 3u);
    REQUIRE_EQUAL(baz1_col.length(), 2u);
    CHECK_EQUAL(baz1_col.Value(0), 2u);
    CHECK(baz1_col.IsNull(1));
  }
  {
    MESSAGE("access foo2");
    CHECK(list_col.IsNull(1));
  }
  {
    MESSAGE("access foo3");
    REQUIRE(!list_col.IsNull(2));
    auto foo3_col_slice = list_col.value_slice(2);
    const auto& foo3_col
      = unbox_ref(caf::get_if<arrow::StructArray>(foo3_col_slice.get()));
    const auto& bar3_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo3_col.field(0).get()));
    const auto& baz3_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo3_col.field(1).get()));
    CHECK_EQUAL(bar3_col.length(), 1u);
    CHECK(bar3_col.IsNull(0));
    REQUIRE_EQUAL(baz3_col.length(), 1u);
    CHECK_EQUAL(baz3_col.Value(0), 6u);
  }
  {
    MESSAGE("access foo4");
    REQUIRE(!list_col.IsNull(3));
    auto foo4_col_slice = list_col.value_slice(3);
    const auto& foo4_col
      = unbox_ref(caf::get_if<arrow::StructArray>(foo4_col_slice.get()));
    const auto& bar4_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo4_col.field(0).get()));
    const auto& baz4_col
      = unbox_ref(caf::get_if<arrow::UInt64Array>(foo4_col.field(1).get()));
    CHECK_EQUAL(bar4_col.length(), 1u);
    CHECK(bar4_col.IsNull(0));
    REQUIRE_EQUAL(baz4_col.length(), 1u);
    CHECK(baz4_col.IsNull(0));
  }
}

TEST(single column - list of record) {
  auto t = list_type{record_type{{"a", string_type{}}}};
  record_type layout{{"values", t}};
  list list1{record{{"a", "123"}}, caf::none};
  auto slice = make_slice(layout, list1, caf::none);
  REQUIRE_EQUAL(slice.rows(), 2u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), make_view(list1));
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - list of strings) {
  auto t = list_type{string_type{}};
  record_type layout{{"values", t}};
  list list1{"hello"s, "world"s};
  list list2{"a"s, "b"s, "c"s};
  auto slice = make_slice(layout, list1, list2, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), make_view(list1));
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), make_view(list2));
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - list of list of integers) {
  auto t = list_type{integer_type{}};
  // Note: we call the copy ctor if we don't wrap legacy_list_type into a type.
  auto llt = list_type{type{t}};
  record_type layout{{"values", llt}};
  list list11{1_i, 2_i, 3_i};
  list list12{10_i, 20_i};
  list list1{list11, list12};
  list list21{};
  list list22{0_i, 1_i, 1_i, 2_i, 3_i, 5_i, 8_i, 13_i};
  list list2{list11, list12};
  auto slice = make_slice(layout, caf::none, list1, list2);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, llt), std::nullopt);
  CHECK_VARIANT_EQUAL(slice.at(1, 0, llt), make_view(list1));
  CHECK_VARIANT_EQUAL(slice.at(2, 0, llt), make_view(list2));
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - map) {
  auto t = map_type{string_type{}, count_type{}};
  record_type layout{{"values", t}};
  map map1{{"foo"s, 42_c}, {"bar"s, 23_c}};
  map map2{{"a"s, 0_c}, {"b"s, 1_c}, {"c", 2_c}};
  auto slice = make_slice(layout, map1, map2, caf::none);
  REQUIRE_EQUAL(slice.rows(), 3u);
  CHECK_VARIANT_EQUAL(slice.at(0, 0, t), make_view(map1));
  CHECK_VARIANT_EQUAL(slice.at(1, 0, t), make_view(map2));
  CHECK_VARIANT_EQUAL(slice.at(2, 0, t), std::nullopt);
  CHECK_ROUNDTRIP(slice);
  record_batch_roundtrip(slice);
}

TEST(single column - serialization) {
  factory<table_slice_builder>::add<experimental_table_slice_builder>(
    table_slice_encoding::experimental);
  auto t = count_type{};
  auto slice1 = make_single_column_slice(t, 0_c, 1_c, 2_c, 3_c);
  decltype(slice1) slice2 = {};
  {
    std::vector<char> buf;
    caf::binary_serializer sink{nullptr, buf};
    CHECK_EQUAL(sink(slice1), caf::none);
    CHECK_EQUAL(detail::legacy_deserialize(buf, slice2), true);
  }
  CHECK_VARIANT_EQUAL(slice2.at(0, 0, t), 0_c);
  CHECK_VARIANT_EQUAL(slice2.at(1, 0, t), 1_c);
  CHECK_VARIANT_EQUAL(slice2.at(2, 0, t), 2_c);
  CHECK_VARIANT_EQUAL(slice2.at(3, 0, t), 3_c);
  CHECK_VARIANT_EQUAL(slice1, slice2);
}

TEST(record batch roundtrip) {
  factory<table_slice_builder>::add<experimental_table_slice_builder>(
    table_slice_encoding::experimental);
  auto t = count_type{};
  auto slice1 = make_single_column_slice(t, 0_c, 1_c, 2_c, 3_c);
  auto batch = to_record_batch(slice1);
  auto slice2 = table_slice{batch, slice1.layout()};
  CHECK_EQUAL(slice1, slice2);
  CHECK_VARIANT_EQUAL(slice2.at(0, 0, t), 0_c);
  CHECK_VARIANT_EQUAL(slice2.at(1, 0, t), 1_c);
  CHECK_VARIANT_EQUAL(slice2.at(2, 0, t), 2_c);
  CHECK_VARIANT_EQUAL(slice2.at(3, 0, t), 3_c);
}

TEST(record batch roundtrip - adding column) {
  factory<table_slice_builder>::add<experimental_table_slice_builder>(
    table_slice_encoding::experimental);
  auto slice1 = make_single_column_slice(count_type{}, 0_c, 1_c, 2_c, 3_c);
  auto batch = to_record_batch(slice1);
  auto cb = experimental_table_slice_builder::column_builder::make(
    type{string_type{}}, arrow::default_memory_pool());
  cb->add("0"sv);
  cb->add("1"sv);
  cb->add("2"sv);
  cb->add("3"sv);
  auto column = cb->finish();
  REQUIRE(column);
  auto new_batch = batch->AddColumn(1, "new", column);
  REQUIRE(new_batch.ok());
  const auto& layout_rt = caf::get<record_type>(slice1.layout());
  auto new_layout_rt = layout_rt.transform(
    {{{layout_rt.num_fields() - 1},
      record_type::insert_after({{"new", string_type{}}})}});
  REQUIRE(new_layout_rt);
  auto new_layout = type{*new_layout_rt};
  new_layout.assign_metadata(slice1.layout());
  auto slice2 = table_slice{new_batch.ValueUnsafe(), new_layout};
  CHECK_VARIANT_EQUAL(slice2.at(0, 0, count_type{}), 0_c);
  CHECK_VARIANT_EQUAL(slice2.at(1, 0, count_type{}), 1_c);
  CHECK_VARIANT_EQUAL(slice2.at(2, 0, count_type{}), 2_c);
  CHECK_VARIANT_EQUAL(slice2.at(3, 0, count_type{}), 3_c);
  CHECK_VARIANT_EQUAL(slice2.at(0, 1, string_type{}), "0"sv);
  CHECK_VARIANT_EQUAL(slice2.at(1, 1, string_type{}), "1"sv);
  CHECK_VARIANT_EQUAL(slice2.at(2, 1, string_type{}), "2"sv);
  CHECK_VARIANT_EQUAL(slice2.at(3, 1, string_type{}), "3"sv);
}

auto field_roundtrip(const type& t) {
  const auto& arrow_field = make_experimental_field({"x", t});
  const auto& restored_t = make_vast_type(*arrow_field);
  CHECK_EQUAL(t, restored_t);
}

TEST(arrow primitive type to field roundtrip) {
  field_roundtrip(type{none_type{}});
  field_roundtrip(type{bool_type{}});
  field_roundtrip(type{integer_type{}});
  field_roundtrip(type{count_type{}});
  field_roundtrip(type{real_type{}});
  field_roundtrip(type{duration_type{}});
  field_roundtrip(type{time_type{}});
  field_roundtrip(type{string_type{}});
  field_roundtrip(type{pattern_type{}});
  field_roundtrip(type{address_type{}});
  field_roundtrip(type{subnet_type{}});
  field_roundtrip(type{enumeration_type{{"first"}, {"third", 2}, {"fourth"}}});
  field_roundtrip(type{list_type{integer_type{}}});
  field_roundtrip(type{map_type{integer_type{}, address_type{}}});
  field_roundtrip(
    type{record_type{{"key", integer_type{}}, {"value", address_type{}}}});
  field_roundtrip(
    type{record_type{{"a", string_type{}}, {"b", address_type{}}}});
  field_roundtrip(type{record_type{
    {"a", string_type{}},
    {"b", record_type{{"hits", count_type{}}, {"net", subnet_type{}}}}}});
}

TEST(arrow names and attrs roundtrip) {
  auto name_n_attrs_type
    = type{"fool", bool_type{}, {{"#key1_novalue"}, {"#key2", "v2"}}};
  auto deeply_nested_type = type{
    "fool",
    type{type{bool_type{}, {{"keyX", "v1"}}},
         {{"#key1_novalue"}, {"#key2", "v2"}}},
  };
  field_roundtrip(type{"fool", bool_type{}});
  field_roundtrip(type{"fool", type{"cool", bool_type{}}});
  field_roundtrip(name_n_attrs_type);
  field_roundtrip(
    type{"fool", type{bool_type{}, {{"#key1_novalue"}, {"#key2", "v2"}}}});
  field_roundtrip(deeply_nested_type);
  field_roundtrip(
    type{"my_list_outer", list_type{type{"inner", deeply_nested_type}}});
  field_roundtrip(type{
    "my_map",
    map_type{
      type{"my_keys", name_n_attrs_type},
      type{"my_vals", deeply_nested_type},
    },
  });
}

auto schema_roundtrip(const type& t) {
  const auto& arrow_schema = make_experimental_schema(t);
  const auto& restored_t = make_vast_type(*arrow_schema);
  CHECK_EQUAL(t, restored_t);
}

TEST(arrow record type to schema roundtrip tp) {
  schema_roundtrip(type{record_type{{"a", integer_type{}}}});
  schema_roundtrip(type{
    "alias",
    record_type{
      {"a", integer_type{}},
      {"b", bool_type{}},
      {"c", integer_type{}},
      {"d", count_type{}},
      {"e", real_type{}},
      {"f", duration_type{}},
      {"g", time_type{}},
      {"h", string_type{}},
      {"i", address_type{}},
      {"j", subnet_type{}},
      {"k", list_type{integer_type{}}},
    },
    {{"top_level_key", "top_level_value"}},
  });
  schema_roundtrip(type{
    record_type{{
      "inner",
      type{record_type{
             {"value", subnet_type{}},
             {"value2", time_type{}},
             {"value3", duration_type{}},
           },
           {{"key0", "value0"}, {"key1"}}},
    }},
  });
  auto inner = type{
    "inner_rec",
    record_type{
      {"a", integer_type{}},
      {"b", string_type{}},
    },
    {{"key0", "value0"}, {"key1"}},
  };
  auto outer = type{
    "outer_rec",
    record_type{
      {"x", count_type{}},
      {"y", string_type{}},
      {"z_nested", inner},
    },
    {{"keyx", "vx"}},
  };
  schema_roundtrip(outer);
}

namespace {

struct fixture : public fixtures::table_slices {
  fixture() : fixtures::table_slices(VAST_PP_STRINGIFY(SUITE)) {
  }
};

} // namespace

FIXTURE_SCOPE(experimental_table_slice_tests, fixture)

TEST_TABLE_SLICE(experimental_table_slice_builder, experimental)

FIXTURE_SCOPE_END()