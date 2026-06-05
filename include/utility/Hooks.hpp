#pragma once

// NEW (vrframework). Minimal self-contained replacements for praydog's
// `utility/{PointerHook,VtableHook,FunctionHook}.hpp` + the small `Address`,
// `ProtectionOverride` and `utility::get_original_bytes` helpers that the
// VERBATIM-ported D3D12Hook.cpp / D3D11Hook.cpp depend on.
//
// REFramework's `utility` lib is a separate submodule that is NOT vendored here, so
// rather than drag it in we re-implement exactly the surface the hook ports use:
//
//   * PointerHook   — overwrite a single function pointer (e.g. a COM vtable slot).
//   * VtableHook    — copy one object's vtable and redirect individual methods.
//   * FunctionHook  — inline detour, backed by safetyhook's InlineHook.
//   * ProtectionOverride / Address — RAII VirtualProtect + a thin address wrapper.
//   * utility::get_original_bytes  — for the reentrancy "restore-original-bytes" escape.
//
// These are pure Win32 / safetyhook; they name ZERO engine types and ZERO RE-Engine
// utilities, so they stay in Layer 1.

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include <windows.h>

#include <safetyhook/inline_hook.hpp>

// ---------------------------------------------------------------------------
// Address — thin wrapper around a pointer/uintptr that converts freely. Mirrors
// the praydog `Address` used by D3D12Hook.cpp (only the conversions it needs).
// ---------------------------------------------------------------------------
class Address {
public:
    Address() = default;
    Address(uintptr_t addr) : m_addr{ addr } {}
    Address(void* addr) : m_addr{ reinterpret_cast<uintptr_t>(addr) } {}

    template <typename T> Address(T* ptr) : m_addr{ reinterpret_cast<uintptr_t>(ptr) } {}

    operator uintptr_t() const { return m_addr; }
    operator void*() const { return reinterpret_cast<void*>(m_addr); }

    uintptr_t as_uintptr() const { return m_addr; }
    void* ptr() const { return reinterpret_cast<void*>(m_addr); }

private:
    uintptr_t m_addr{ 0 };
};

// ---------------------------------------------------------------------------
// ProtectionOverride — RAII VirtualProtect; restores the previous protection.
// ---------------------------------------------------------------------------
class ProtectionOverride {
public:
    ProtectionOverride(void* address, size_t size, uint32_t protection)
        : m_address{ address }, m_size{ size } {
        VirtualProtect(m_address, m_size, protection, &m_old);
    }

    ~ProtectionOverride() {
        DWORD ignored{};
        VirtualProtect(m_address, m_size, m_old, &ignored);
    }

private:
    void* m_address{ nullptr };
    size_t m_size{ 0 };
    DWORD m_old{ 0 };
};

namespace utility {
// Returns the on-disk/original bytes of a function IF it currently appears hooked.
// The hook ports use this only as a reentrancy safety net: when recursion is detected
// they temporarily restore the original bytes so the nested call makes forward progress.
//
// We track originals through the FunctionHook registry below; a plain pointer/vtable
// patch has no inline trampoline, so this returns nullopt for those (the ports already
// guard on that). Defined inline so the header is header-only.
std::optional<std::vector<uint8_t>> get_original_bytes(Address address);
} // namespace utility

// ---------------------------------------------------------------------------
// PointerHook — overwrite a single pointer slot (a COM vtable entry) with our
// detour and remember the original so callers can call through.
// ---------------------------------------------------------------------------
class PointerHook {
public:
    PointerHook(void** ptr, void* destination) : m_ptr{ ptr } {
        m_original = *m_ptr;

        ProtectionOverride ov{ m_ptr, sizeof(void*), PAGE_READWRITE };
        *m_ptr = destination;
    }

    template <typename T> PointerHook(T** ptr, void* destination)
        : PointerHook{ reinterpret_cast<void**>(ptr), destination } {}

    virtual ~PointerHook() { remove(); }

    bool remove() {
        if (m_ptr != nullptr && m_original != nullptr) {
            ProtectionOverride ov{ m_ptr, sizeof(void*), PAGE_READWRITE };
            *m_ptr = m_original;
        }
        return true;
    }

    template <typename T> T get_original() const { return reinterpret_cast<T>(m_original); }

private:
    void** m_ptr{ nullptr };
    void* m_original{ nullptr };
};

// ---------------------------------------------------------------------------
// VtableHook — copy a single object's vtable into our own storage, point the
// object at the copy, and redirect individual methods. Per-instance (the least
// intrusive option — does not touch the shared vtable other overlays read).
// ---------------------------------------------------------------------------
class VtableHook {
public:
    VtableHook() = default;

    explicit VtableHook(void* instance) { create(instance); }

    virtual ~VtableHook() { remove(); }

    bool create(void* instance) {
        if (instance == nullptr) {
            return false;
        }

        m_instance = reinterpret_cast<void***>(instance);
        m_original_vtable = *m_instance;

        // Count the vtable entries (cap to a sane maximum).
        m_vtable_size = 0;
        for (size_t i = 0; i < kMaxMethods; ++i) {
            if (IsBadCodePtr(reinterpret_cast<FARPROC>(m_original_vtable[i]))) {
                break;
            }
            ++m_vtable_size;
        }

        if (m_vtable_size == 0) {
            return false;
        }

        m_new_vtable = std::make_unique<void*[]>(m_vtable_size);
        std::memcpy(m_new_vtable.get(), m_original_vtable, m_vtable_size * sizeof(void*));

        ProtectionOverride ov{ m_instance, sizeof(void*), PAGE_READWRITE };
        *m_instance = m_new_vtable.get();
        return true;
    }

    bool hook_method(uint32_t index, uintptr_t new_method) {
        if (m_new_vtable == nullptr || index >= m_vtable_size) {
            return false;
        }
        m_new_vtable[index] = reinterpret_cast<void*>(new_method);
        return true;
    }

    template <typename T> T get_method(uint32_t index) const {
        if (m_original_vtable == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<T>(m_original_vtable[index]);
    }

    void* get_instance() const { return reinterpret_cast<void*>(m_instance); }

    bool remove() {
        if (m_instance != nullptr && m_original_vtable != nullptr) {
            ProtectionOverride ov{ m_instance, sizeof(void*), PAGE_READWRITE };
            *m_instance = m_original_vtable;
        }
        m_new_vtable.reset();
        m_instance = nullptr;
        m_original_vtable = nullptr;
        m_vtable_size = 0;
        return true;
    }

private:
    static constexpr size_t kMaxMethods = 512;

    void*** m_instance{ nullptr };
    void** m_original_vtable{ nullptr };
    std::unique_ptr<void*[]> m_new_vtable{};
    size_t m_vtable_size{ 0 };
};

// ---------------------------------------------------------------------------
// FunctionHook — inline detour over a real function address, backed by safetyhook.
// Used only for the Streamline linkSwapchainToCmdQueue hook in D3D12Hook.cpp.
// ---------------------------------------------------------------------------
class FunctionHook {
public:
    FunctionHook(Address target, uintptr_t destination)
        : m_target{ target }, m_destination{ destination } {}

    template <typename T> FunctionHook(Address target, T* destination)
        : FunctionHook{ target, reinterpret_cast<uintptr_t>(destination) } {}

    virtual ~FunctionHook() = default;

    bool create() {
        auto result = safetyhook::InlineHook::create(m_target.ptr(), reinterpret_cast<void*>(m_destination));
        if (!result) {
            return false;
        }
        m_hook = std::make_unique<safetyhook::InlineHook>(std::move(*result));
        return static_cast<bool>(*m_hook);
    }

    template <typename T> T get_original() const {
        return m_hook != nullptr ? m_hook->original<T>() : nullptr;
    }

    const std::vector<uint8_t>& original_bytes() const {
        static const std::vector<uint8_t> empty{};
        return m_hook != nullptr ? m_hook->original_bytes() : empty;
    }

    safetyhook::InlineHook* raw() const { return m_hook.get(); }

private:
    Address m_target{};
    uintptr_t m_destination{ 0 };
    std::unique_ptr<safetyhook::InlineHook> m_hook{};
};
