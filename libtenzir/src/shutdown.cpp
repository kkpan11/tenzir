//    _   _____   __________
//   | | / / _ | / __/_  __/     Visibility
//   | |/ / __ |_\ \  / /          Across
//   |___/_/ |_/___/ /_/       Space and Time
//
// SPDX-FileCopyrightText: (c) 2020 The Tenzir Contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "tenzir/shutdown.hpp"

#include "tenzir/fwd.hpp"

#include "tenzir/atoms.hpp"
#include "tenzir/detail/assert.hpp"
#include "tenzir/detail/type_traits.hpp"
#include "tenzir/die.hpp"
#include "tenzir/logger.hpp"
#include "tenzir/terminate.hpp"

#include <caf/response_promise.hpp>

namespace tenzir {

template <class Policy>
void shutdown(caf::event_based_actor* self, std::vector<caf::actor> xs,
              caf::error reason) {
  // Terminate actors as requested.
  terminate<Policy>(self, std::move(xs))
    .then(
      [=, reason = std::move(reason)](atom::done) mutable {
        TENZIR_DEBUG("{} terminates after shutting down all dependents", *self);
        self->quit(std::move(reason));
      },
      [=](const caf::error& err) {
        die(
          fmt::format("failed to cleanly terminate dependent actors: {}", err));
      });
}

template void
shutdown<policy::sequential>(caf::event_based_actor*, std::vector<caf::actor>,
                             caf::error reason);

template void
shutdown<policy::parallel>(caf::event_based_actor*, std::vector<caf::actor>,
                           caf::error reason);

template <class Policy>
void shutdown(caf::scoped_actor& self, std::vector<caf::actor> xs,
              caf::error reason) {
  terminate<Policy>(self, std::move(xs))
    .receive(
      [&](atom::done) {
        TENZIR_DEBUG("{} terminates after shutting down all dependents", *self);
        self->send_exit(self, std::move(reason));
      },
      [&](const caf::error& err) {
        die(
          fmt::format("failed to cleanly terminate dependent actors: {}", err));
      });
}

template void
shutdown<policy::sequential>(caf::scoped_actor&, std::vector<caf::actor>,
                             caf::error reason);

template void
shutdown<policy::parallel>(caf::scoped_actor&, std::vector<caf::actor>,
                           caf::error reason);

} // namespace tenzir
