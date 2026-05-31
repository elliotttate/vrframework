#include "memory/memory_mul.h"

#include <vector>
#include <windows.h>
#include <spdlog/spdlog.h>

namespace memory {

uintptr_t module_base() {
    static uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    return base;
}

size_t module_size() {
    static size_t size = [] {
        auto base = (HMODULE)module_base();
        MODULEINFO mi{};
        if (GetModuleInformation(GetCurrentProcess(), base, &mi, sizeof(mi))) {
            return (size_t)mi.SizeOfImage;
        }
        return (size_t)0;
    }();
    return size;
}

// --- IDA-style pattern parsing + scan ---------------------------------------
namespace {
struct PatByte { uint8_t value; bool wildcard; };

std::vector<PatByte> parse(std::string_view p) {
    std::vector<PatByte> out;
    for (size_t i = 0; i < p.size();) {
        char c = p[i];
        if (c == ' ') { ++i; continue; }
        if (c == '?') {
            out.push_back({ 0, true });
            i += (i + 1 < p.size() && p[i + 1] == '?') ? 2 : 1;
        } else {
            auto hex = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            int hi = hex(p[i]), lo = (i + 1 < p.size()) ? hex(p[i + 1]) : -1;
            out.push_back({ (uint8_t)((hi << 4) | (lo < 0 ? 0 : lo)), false });
            i += 2;
        }
    }
    return out;
}
} // namespace

std::optional<uintptr_t> scan(std::string_view pattern) {
    const auto pat = parse(pattern);
    if (pat.empty()) return std::nullopt;

    const auto base = module_base();
    const auto size = module_size();
    if (!base || !size || size < pat.size()) return std::nullopt;

    const auto* data = reinterpret_cast<const uint8_t*>(base);
    for (size_t i = 0; i + pat.size() <= size; ++i) {
        bool match = true;
        for (size_t j = 0; j < pat.size(); ++j) {
            if (!pat[j].wildcard && data[i + j] != pat[j].value) { match = false; break; }
        }
        if (match) return base + i;
    }
    return std::nullopt;
}

// --- relocation helpers ------------------------------------------------------
#if defined(_DEBUG) || defined(SIGNATURE_SCAN)
static constexpr bool kDoScan = true;
#else
static constexpr bool kDoScan = false;
#endif

uintptr_t FuncRelocation(const char* name, const char* pattern, uintptr_t fallback) {
    if constexpr (kDoScan) {
        if (auto ref = scan(pattern)) {
            const auto off = *ref - module_base();
            if (fallback && off != fallback) {
                spdlog::warn("FuncRelocation '{}' mismatch: scanned={:x} fallback={:x}", name, off, fallback);
            }
            return *ref;
        }
        spdlog::error("FuncRelocation '{}': pattern not found", name);
    }
    if (fallback) return fallback + module_base();
    spdlog::error("FuncRelocation '{}': no fallback offset provided", name);
    return 0;
}

uintptr_t InstructionRelocation(const char* name, const char* pattern,
                                unsigned offset_begin, unsigned instruction_size,
                                uintptr_t fallback) {
    if constexpr (kDoScan) {
        if (auto ref = scan(pattern)) {
            const auto addr = *ref;
            const auto val = addr + *reinterpret_cast<int32_t*>(addr + offset_begin) + instruction_size;
            const auto off = val - module_base();
            if (fallback && off != fallback) {
                spdlog::warn("InstructionRelocation '{}' mismatch: scanned={:x} fallback={:x}", name, off, fallback);
            }
            return val;
        }
        spdlog::error("InstructionRelocation '{}': pattern not found", name);
    }
    if (fallback) return fallback + module_base();
    spdlog::error("InstructionRelocation '{}': no fallback offset provided", name);
    return 0;
}

uintptr_t VTable(const char* name, const char* type_name, uintptr_t fallback) {
    // PORT FROM: praydog utility/RTTI.hpp find_vtable(). Until then, trust fallback.
    if (fallback) return fallback + module_base();
    spdlog::error("VTable '{}' ({}): RTTI scan not implemented and no fallback", name, type_name);
    return 0;
}

// --- OffsetManifest (Layer-3 improvement) -----------------------------------
// PORT/EXTEND: parse a real toml/ini. This minimal version is enough to wire the API;
// fill in a parser (e.g. tomlplusplus) to ship per-game manifests.
OffsetManifest& OffsetManifest::get() {
    static OffsetManifest inst{};
    return inst;
}
bool OffsetManifest::load(const std::string& path) {
    spdlog::info("OffsetManifest::load('{}') — TODO: parse manifest", path);
    return false;
}
uintptr_t OffsetManifest::resolve(const std::string&) const { return 0; }
bool OffsetManifest::has(const std::string&) const { return false; }

} // namespace memory
