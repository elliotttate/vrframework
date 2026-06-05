#include "hooks/D3D11Hook.hpp"

// PORT FROM: REFramework/src/D3D11Hook.cpp — engine-agnostic, copy verbatim. STUB.
//
// PointerHook is only forward-declared in the header (to keep <safetyhook> out of it); the
// unique_ptr<PointerHook> member needs the COMPLETE type wherever the destructor is instantiated.
#include "utility/Hooks.hpp"

D3D11Hook::~D3D11Hook() = default;

bool D3D11Hook::hook() { return false; }   // PORT
bool D3D11Hook::unhook() { return false; } // PORT

HRESULT WINAPI D3D11Hook::present(IDXGISwapChain*, UINT, UINT) { return S_OK; }
HRESULT WINAPI D3D11Hook::resize_buffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) { return S_OK; }
