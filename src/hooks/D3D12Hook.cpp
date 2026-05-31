#include "hooks/D3D12Hook.hpp"

// PORT FROM: REFramework/src/D3D12Hook.cpp — engine-agnostic, copy verbatim.
// STUB: implements the destructor only so the static lib links. Lift the real present/
// resize hooking (dummy swapchain vtable walk + PointerHook on Present) from REFramework.

D3D12Hook::~D3D12Hook() = default;

bool D3D12Hook::hook() { return false; }   // PORT
bool D3D12Hook::unhook() { return false; } // PORT

HRESULT WINAPI D3D12Hook::present(IDXGISwapChain3*, uint64_t, uint64_t, void*) { return S_OK; }
HRESULT WINAPI D3D12Hook::resize_buffers(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT) { return S_OK; }
HRESULT WINAPI D3D12Hook::resize_target(IDXGISwapChain3*, const DXGI_MODE_DESC*) { return S_OK; }
