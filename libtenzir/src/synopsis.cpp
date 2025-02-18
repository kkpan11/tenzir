//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2018 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/synopsis.hpp"

#include "tenzir/bool_synopsis.hpp"
#include "tenzir/detail/legacy_deserialize.hpp"
#include "tenzir/detail/overload.hpp"
#include "tenzir/error.hpp"
#include "tenzir/fbs/utils.hpp"
#include "tenzir/logger.hpp"
#include "tenzir/qualified_record_field.hpp"
#include "tenzir/synopsis_factory.hpp"
#include "tenzir/time_synopsis.hpp"

#include <caf/binary_serializer.hpp>
#include <caf/deserializer.hpp>
#include <caf/error.hpp>
#include <caf/sec.hpp>

#include <typeindex>

namespace tenzir {

namespace {

caf::expected<synopsis_ptr> unpack_opaque_synopsis(
  const tenzir::fbs::synopsis::LegacyOpaqueSynopsis& synopsis) {
  auto ret = synopsis_ptr{nullptr};
  if (auto data = synopsis.caf_0_18_data()) {
    caf::binary_deserializer sink(data->data(), data->size());
    if (sink.apply(ret)) {
      return ret;
    }
    return caf::make_error(ec::parse_error, "opaque_synopsis(0_18) not "
                                            "deserializable");
  }
  return caf::make_error(ec::parse_error, "Lack of data in "
                                          "opaque_synopsis. Unable to "
                                          "deserialize");
}

} // namespace

synopsis::synopsis(tenzir::type x) : type_{std::move(x)} {
  // nop
}

const tenzir::type& synopsis::type() const {
  return type_;
}

synopsis_ptr synopsis::shrink() const {
  return nullptr;
}

caf::expected<flatbuffers::Offset<fbs::synopsis::LegacySynopsis>>
pack(flatbuffers::FlatBufferBuilder& builder, const synopsis_ptr& synopsis,
     const qualified_record_field& fqf) {
  auto column_name = fbs::serialize_bytes(builder, fqf);
  if (!column_name) {
    return column_name.error();
  }
  auto* ptr = synopsis.get();
  if (auto* tptr = dynamic_cast<time_synopsis*>(ptr)) {
    auto min = tptr->min().time_since_epoch().count();
    auto max = tptr->max().time_since_epoch().count();
    fbs::synopsis::TimeSynopsis time_synopsis(min, max);
    fbs::synopsis::LegacySynopsisBuilder synopsis_builder(builder);
    synopsis_builder.add_qualified_record_field(*column_name);
    synopsis_builder.add_time_synopsis(&time_synopsis);
    return synopsis_builder.Finish();
  }
  if (auto* bptr = dynamic_cast<bool_synopsis*>(ptr)) {
    fbs::synopsis::BoolSynopsis bool_synopsis(bptr->any_true(),
                                              bptr->any_false());
    fbs::synopsis::LegacySynopsisBuilder synopsis_builder(builder);
    synopsis_builder.add_qualified_record_field(*column_name);
    synopsis_builder.add_bool_synopsis(&bool_synopsis);
    return synopsis_builder.Finish();
  } else {
    auto data = fbs::serialize_bytes(builder, synopsis);
    if (!data) {
      return data.error();
    }
    fbs::synopsis::LegacyOpaqueSynopsisBuilder opaque_builder(builder);
    opaque_builder.add_caf_0_18_data(*data);
    auto opaque_synopsis = opaque_builder.Finish();
    fbs::synopsis::LegacySynopsisBuilder synopsis_builder(builder);
    synopsis_builder.add_qualified_record_field(*column_name);
    synopsis_builder.add_opaque_synopsis(opaque_synopsis);
    return synopsis_builder.Finish();
  }
  return caf::make_error(ec::logic_error, "unreachable");
}

caf::error
unpack(const fbs::synopsis::LegacySynopsis& synopsis, synopsis_ptr& ptr) {
  ptr = nullptr;
  if (auto bs = synopsis.bool_synopsis()) {
    ptr = std::make_unique<bool_synopsis>(bs->any_true(), bs->any_false());
  } else if (auto ts = synopsis.time_synopsis()) {
    ptr = std::make_unique<time_synopsis>(
      tenzir::time{} + tenzir::duration{ts->start()},
      tenzir::time{} + tenzir::duration{ts->end()});
  } else if (auto os = synopsis.opaque_synopsis()) {
    if (auto synopsis = unpack_opaque_synopsis(*os)) {
      ptr = std::move(*synopsis);
    } else {
      return std::move(synopsis.error());
    }
  } else {
    return caf::make_error(ec::format_error, "no synopsis type");
  }
  return caf::none;
}

synopsis_ptr make_synopsis(const type& t) {
  return factory<synopsis>::make(t, caf::settings{});
}

} // namespace tenzir
