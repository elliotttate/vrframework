#pragma once

// PORT FROM: starfield2vr ScanHelper.h + praydog utility/Scan.hpp.
// Address resolution used by the ports' per-game offsets.h. Signatures match the
// anvil call sites:  FuncRelocation("name", pattern, fallback)
//                    InstructionRelocation("name", pattern, off, len, fallback)
//
// Behavior: when built with SIGNATURE_SCAN (or _DEBUG) it pattern-scans the main module
// and warns on fallback mismatch; otherwise it trusts the fallback offset. The manifest
// loader below is the Layer-3 improvement: lift offsets out of compiled .h into a file
// so one binary per engine serves every title.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace memory {

// Module helpers (impl: src/memory/memory_mul.cpp).
uintptr_t module_base();
size_t    module_size();

// Scan the main module for an IDA-style byte pattern ("48 8B ? ? E8").
std::optional<uintptr_t> scan(std::string_view pattern);

// Resolve a function address. `pattern` is scanned (signature-scan builds); `fallback`
// is a module-relative offset trusted in release. Warns if scanned != fallback.
uintptr_t FuncRelocation(const char* name, const char* pattern, uintptr_t fallback = 0);

// Resolve a RIP-relative operand at `pattern`: addr + *(int32*)(addr+offset_begin) +
// instruction_size. Used for globals referenced via lea/mov (see anvil g_*_addr()).
uintptr_t InstructionRelocation(const char* name, const char* pattern,
                                unsigned offset_begin, unsigned instruction_size,
                                uintptr_t fallback = 0);

// Resolve a vtable by RTTI type name.
uintptr_t VTable(const char* name, const char* type_name, uintptr_t fallback = 0);

// ---- Layer-3 improvement: external offsets manifest -------------------------
// Replace compiled offsets.h with a loaded file (toml/ini). Look up a named offset
// (pattern+fallback) at runtime so a single engine binary supports every game build.
struct OffsetEntry {
    std::string pattern;
    uintptr_t   fallback{ 0 };
};
class OffsetManifest {
public:
    static OffsetManifest& get();
    bool load(const std::string& path);          // e.g. "games/valhalla.toml"
    uintptr_t resolve(const std::string& name) const;  // scan-or-fallback by entry name
    bool has(const std::string& name) const;
};

} // namespace memory
