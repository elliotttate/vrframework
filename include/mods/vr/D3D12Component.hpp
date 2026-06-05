#pragma once

// PORT FROM: REFramework/src/mods/vr/D3D12Component.{cpp,hpp}
// The D3D12 stereo plumbing for the OpenXR path: create per-eye OpenXR swapchains, copy
// the finished engine backbuffer (this AFR frame's eye image) into the acquired runtime
// swapchain image with the correct barriers/fence, and drive xrBeginFrame/xrEndFrame.
//
// SIMPLIFICATION vs REFramework: REFramework leaned on its `d3d12::TextureContext` /
// `ResourceCopier` subsystem + DirectXTK SpriteBatch (for HDR->8bit conversion). To keep
// vrframework's Layer-1 self-contained, this version bakes in a minimal barrier/copy/fence
// helper (ResourceCopier) modeled on the validated XrSimTest.cpp submit loop. The OpenVR
// branch is omitted here (OpenXR is the FH5 target); add it back from REFramework when
// wiring OpenVR. HDR->8bit conversion is left as a TODO hook (most engines present 8-bit;
// FH5's backbuffer format is validated at setup()).

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

#include "vr/runtimes/OpenXRDispatch.hpp"   // dynamic XR dispatch (XR_NO_PROTOTYPES) — see header

class VR;

namespace vrmod {

// Minimal barrier->copy->barrier helper with its own allocator/list/fence, one per
// in-flight slot. Equivalent in spirit to REFramework's d3d12::ResourceCopier; the
// barrier discipline matches guide 10 §3 (restore source/dest states after the copy).
struct ResourceCopier {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    bool setup(ID3D12Device* device, const wchar_t* name = nullptr);
    void reset();
    void wait(uint32_t ms);
    // CopyResource src->dst with symmetric barrier pairs; states are restored on return.
    void copy(ID3D12Resource* src, ID3D12Resource* dst,
              D3D12_RESOURCE_STATES src_state, D3D12_RESOURCE_STATES dst_state);
    void execute(ID3D12CommandQueue* queue);

    ComPtr<ID3D12CommandAllocator> allocator{};
    ComPtr<ID3D12GraphicsCommandList> cmd_list{};
    ComPtr<ID3D12Fence> fence{};
    HANDLE fence_event{nullptr};
    UINT64 fence_value{0};
    bool waiting{false};
};

class D3D12Component {
public:
    // Per-frame entry: copy this eye's backbuffer into the matching OpenXR swapchain image,
    // and on the second eye of the AFR pair drive xrBegin/xrEndFrame. Returns true on submit.
    bool on_frame(VR* vr);
    void on_post_present(VR* vr);
    void on_reset(VR* vr);

    void force_reset() { m_force_reset = true; }
    const auto& get_backbuffer_size() const { return m_backbuffer_size; }
    auto is_initialized() const { return m_openxr.contexts.size() > 0; }
    auto& openxr() { return m_openxr; }

private:
    void setup();

    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct OpenXR {
        std::optional<std::string> create_swapchains(VR* vr);
        void destroy_swapchains(VR* vr);
        // Acquire/wait/copy/release the swapchain image for eye `swapchain_idx`.
        void copy(VR* vr, uint32_t swapchain_idx, ID3D12Resource* src,
                  ID3D12Device* device, ID3D12CommandQueue* queue);
        void wait_for_all_copies();

        struct SwapchainContext {
            std::vector<XrSwapchainImageD3D12KHR> textures{};
            std::vector<std::unique_ptr<ResourceCopier>> copiers{};
            uint32_t num_textures_acquired{0};
        };

        std::vector<SwapchainContext> contexts{};
        std::recursive_mutex mtx{};
        std::array<uint32_t, 2> last_resolution{};
    } m_openxr;

    std::array<ResourceCopier, 3> m_generic_copiers{}; // desktop-mirror / scratch copies
    ComPtr<ID3D12Resource> m_prev_backbuffer{};

    uint32_t m_backbuffer_size[2]{};
    DXGI_FORMAT m_backbuffer_format{DXGI_FORMAT_R8G8B8A8_UNORM};
    bool m_force_reset{false};
};
} // namespace vrmod
