//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2016 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "tenzir/actors.hpp"
#include "tenzir/component_registry.hpp"
#include "tenzir/series_builder.hpp"

#include <caf/actor.hpp>
#include <caf/stateful_actor.hpp>
#include <caf/typed_event_based_actor.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace tenzir {

/// State of the node actor.
struct node_state {
  // -- rest handling infrastructure -------------------------------------------

  using handler_and_endpoint = std::pair<rest_handler_actor, rest_endpoint>;

  /// Retrieve or spawn the handler actor for the given request.
  auto get_endpoint_handler(const http_request_description& desc)
    -> const handler_and_endpoint&;

  /// The REST endpoint handlers for this node. Spawned on demand.
  std::unordered_map<std::string, handler_and_endpoint> rest_handlers;

  // -- actor facade -----------------------------------------------------------

  /// The name of the NODE actor.
  constexpr static auto name = "node";

  /// A pointer to the NODE actor handle.
  node_actor::pointer self = {};

  // -- member types -----------------------------------------------------------

  /// Stores the base directory for persistent state.
  std::filesystem::path dir;

  /// The component registry.
  component_registry registry = {};

  /// The list of component plugin actors in the order that they were spawned.
  std::vector<std::string> ordered_components;

  /// Components that are still alive for lifetime-tracking.
  std::set<std::pair<caf::actor_addr, std::string>> alive_components;

  /// Map from component actor address to name for better error messages. Never
  /// cleared.
  std::unordered_map<caf::actor_addr, std::string> component_names;

  /// Counters for multi-instance components.
  std::unordered_map<std::string, uint64_t> label_counters;

  /// Builder for API metrics.
  std::unordered_map<std::string, series_builder> api_metrics_builders;

  /// Startup timestamp.
  time start_time = time::clock::now();

  /// Flag to signal if the node received an exit message.
  bool tearing_down = false;

  /// Weak handles to remotely spawned and monitored exec ndoes for cleanup on
  /// node shutdown.
  std::unordered_set<caf::actor_addr> monitored_exec_nodes;
};

/// Spawns a node.
/// @param self The actor handle
/// @param dir The directory where to store persistent state.
node_actor::behavior_type
node(node_actor::stateful_pointer<node_state> self, std::filesystem::path dir);

} // namespace tenzir
