#pragma once

// Authored from the get<T>/set<T> usage in the ports' ModValue + Mod config paths.
// Swap for praydog's utility/Config.hpp if you vendor the `utility` submodule.
// A flat key/value INI-style store with typed accessors. Body: src/utility/Config.cpp.

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace utility {
class Config {
public:
    Config() = default;
    explicit Config(const std::string& file) { load(file); }

    bool load(const std::string& file);
    bool save(const std::string& file) const;

    // Raw string access.
    std::optional<std::string> get(std::string_view key) const;
    void set(std::string_view key, std::string_view value);

    // Typed access used by ModValue<T> (bool / int32_t / float / std::string).
    template <typename T> std::optional<T> get(std::string_view key) const;
    template <typename T> void set(std::string_view key, const T& value);

    const auto& entries() const { return m_entries; }

private:
    std::unordered_map<std::string, std::string> m_entries{};
};
} // namespace utility
