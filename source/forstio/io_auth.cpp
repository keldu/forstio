#include "io_auth.h"

namespace saw {
Peer::Peer(const std::string &identity_) : identity_value{identity_} {}
Peer::Peer(std::string &&identity_) : identity_value{std::move(identity_)} {}

const std::string &Peer::identity() const { return identity_value; }
} // namespace saw
