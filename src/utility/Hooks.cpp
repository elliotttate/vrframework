#include "utility/Hooks.hpp"

// utility::get_original_bytes — see header. This is used by the D3D11/D3D12 present &
// resize detours ONLY as a reentrancy safety net: when recursion is detected they ask
// for the on-disk bytes of the (vtable) function so they can temporarily restore them.
//
// In this build the present/resize functions are redirected via per-instance VtableHook
// (a pointer swap), NOT an inline byte patch — so the bytes at the function address are
// already the game's original. Returning nullopt makes the ports skip the restore and
// fall through to "just call through once / return S_OK", which is the correct behavior
// for a pointer/vtable redirect. (REFramework's util returns bytes only when MinHook has
// actually patched the prologue.)

namespace utility {
std::optional<std::vector<uint8_t>> get_original_bytes(Address /*address*/) {
    return std::nullopt;
}
} // namespace utility
