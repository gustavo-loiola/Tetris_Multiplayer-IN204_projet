#pragma once

#include <string>
#include <optional>
#include "network/MessageTypes.hpp"

namespace tetris::net {

/// Serialize a Message into a single line of UTF-8 text (without trailing '\n').
std::string serialize(const Message& msg);

/// Parse a Message from a single line of UTF-8 text (no trailing '\n' required).
/// Returns std::nullopt on parse error.
std::optional<Message> deserialize(const std::string& line);

} // namespace tetris::net
