#include "utility/Config.hpp"

#include <charconv>
#include <fstream>
#include <sstream>

namespace utility {

bool Config::load(const std::string& file) {
    std::ifstream in{ file };
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            const auto a = s.find_first_not_of(" \t\r\n");
            const auto b = s.find_last_not_of(" \t\r\n");
            return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
        };
        m_entries[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return true;
}

bool Config::save(const std::string& file) const {
    std::ofstream out{ file };
    if (!out) return false;
    for (const auto& [k, v] : m_entries) out << k << " = " << v << '\n';
    return true;
}

std::optional<std::string> Config::get(std::string_view key) const {
    const auto it = m_entries.find(std::string{ key });
    if (it == m_entries.end()) return std::nullopt;
    return it->second;
}

void Config::set(std::string_view key, std::string_view value) {
    m_entries[std::string{ key }] = std::string{ value };
}

// --- typed specializations used by ModValue<T> ------------------------------
template <> std::optional<std::string> Config::get<std::string>(std::string_view key) const {
    return get(key);
}
template <> std::optional<bool> Config::get<bool>(std::string_view key) const {
    auto v = get(key);
    if (!v) return std::nullopt;
    return (*v == "1" || *v == "true" || *v == "True");
}
template <> std::optional<int32_t> Config::get<int32_t>(std::string_view key) const {
    auto v = get(key);
    if (!v) return std::nullopt;
    int32_t out{};
    auto [p, ec] = std::from_chars(v->data(), v->data() + v->size(), out);
    if (ec != std::errc{}) return std::nullopt;
    return out;
}
template <> std::optional<float> Config::get<float>(std::string_view key) const {
    auto v = get(key);
    if (!v) return std::nullopt;
    try { return std::stof(*v); } catch (...) { return std::nullopt; }
}

// NOTE: each wraps the value in std::string_view so the call binds to the NON-template
// set(string_view, string_view) overload. Passing a bare std::string/const char* would exact-match the
// template set<T>(string_view, const T&) instead, instantiating set<std::string>/set<const char*>
// (the latter has no definition -> LNK2019; the former would self-recurse).
template <> void Config::set<std::string>(std::string_view key, const std::string& value) {
    set(key, std::string_view{ value });
}
template <> void Config::set<bool>(std::string_view key, const bool& value) {
    set(key, std::string_view{ value ? "true" : "false" });
}
template <> void Config::set<int32_t>(std::string_view key, const int32_t& value) {
    const std::string s = std::to_string(value);
    set(key, std::string_view{ s });
}
template <> void Config::set<float>(std::string_view key, const float& value) {
    std::ostringstream ss; ss << value;
    const std::string s = ss.str();
    set(key, std::string_view{ s });
}

} // namespace utility
