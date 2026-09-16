// Rendering of a SystemReport for humans (text) and machines (JSON).
#pragma once

#include <string>

#include "core/model.hpp"

namespace sysdiag {

// Current JSON document layout version. Bump on breaking changes.
inline constexpr std::uint64_t kJsonSchemaVersion = 1;

[[nodiscard]] std::string format_text(const SystemReport& report);
[[nodiscard]] std::string format_json(const SystemReport& report);

}  // namespace sysdiag
