// PORT FROM: REFramework/src/mods/vr/D3D12Component.cpp (OpenXR D3D12 path only).
// See header for the simplification rationale. The acquire/wait/copy/release + xrEndFrame
// sequence here is the one validated in FH5CameraProbe/src/XrSimTest.cpp.

#include <spdlog/spdlog.h>

#include "Framework.hpp"
#include "mods/VR.hpp"
#include "mods/vr/D3D12Component.hpp"

namespace vrmod {

// ===========================================================================
// ResourceCopier — self-contained barrier/copy/fence (guide 10 §3 discipline)
// ===========================================================================
bool ResourceCopier::setup(ID3D12Device* device, const wchar_t* name) {
    reset();

    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) {
        spdlog::error("[VR] ResourceCopier: CreateCommandAllocator failed");
        return false;
    }

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list)))) {
        spdlog::error("[VR] ResourceCopier: CreateCommandList failed");
        return false;
    }
    cmd_list->Close();

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        spdlog::error("[VR] ResourceCopier: CreateFence failed");
        return false;
    }

    fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    fence_value = 0;
    waiting = false;

    if (name != nullptr && cmd_list != nullptr) {
        cmd_list->SetName(name);
    }

    return true;
}

void ResourceCopier::reset() {
    if (fence_event != nullptr) {
        wait(INFINITE);
        CloseHandle(fence_event);
        fence_event = nullptr;
    }
    allocator.Reset();
    cmd_list.Reset();
    fence.Reset();
    fence_value = 0;
    waiting = false;
}

void ResourceCopier::wait(uint32_t ms) {
    if (fence == nullptr || fence_event == nullptr || !waiting) {
        return;
    }

    if (fence->GetCompletedValue() < fence_value) {
        fence->SetEventOnCompletion(fence_value, fence_event);
        WaitForSingleObject(fence_event, ms);
    }

    waiting = false;
}

void ResourceCopier::copy(ID3D12Resource* src, ID3D12Resource* dst,
                          D3D12_RESOURCE_STATES src_state, D3D12_RESOURCE_STATES dst_state) {
    if (allocator == nullptr || cmd_list == nullptr) {
        return;
    }

    allocator->Reset();
    cmd_list->Reset(allocator.Get(), nullptr);

    D3D12_RESOURCE_BARRIER barriers[2]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = src;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = src_state;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dst;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = dst_state;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    cmd_list->ResourceBarrier(2, barriers);
    cmd_list->CopyResource(dst, src);

    // Restore both resources to their incoming states so the engine/runtime find them as left.
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = src_state;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = dst_state;
    cmd_list->ResourceBarrier(2, barriers);

    cmd_list->Close();
}

void ResourceCopier::execute(ID3D12CommandQueue* queue) {
    if (cmd_list == nullptr || queue == nullptr) {
        return;
    }

    ID3D12CommandList* lists[] = {cmd_list.Get()};
    queue->ExecuteCommandLists(1, lists);
    queue->Signal(fence.Get(), ++fence_value);
    waiting = true;
}

// ===========================================================================
// D3D12Component
// ===========================================================================
bool D3D12Component::on_frame(VR* vr) {
    if (m_force_reset || m_backbuffer_size[0] == 0) {
        setup();
    }

    auto& hook = g_framework->get_d3d12_hook();
    auto device = hook->get_device();
    auto command_queue = hook->get_command_queue();
    auto swapchain = hook->get_swap_chain();

    if (device == nullptr || command_queue == nullptr || swapchain == nullptr) {
        return false;
    }

    auto runtime = vr->get_runtime();
    if (runtime == nullptr || !runtime->is_openxr() || vr->m_openxr == nullptr || !vr->m_openxr->ready()) {
        return false;
    }

    // Get the finished eye image (the engine just rendered this AFR frame's eye into it).
    ComPtr<ID3D12Resource> backbuffer{};
    const auto backbuffer_index = swapchain->GetCurrentBackBufferIndex();
    if (FAILED(swapchain->GetBuffer(backbuffer_index, IID_PPV_ARGS(&backbuffer))) || backbuffer == nullptr) {
        spdlog::error("[VR] Failed to get back buffer");
        return false;
    }

    // NOTE (HDR->8bit): REFramework converts non-8-bit backbuffers to R8G8B8A8 via a
    // SpriteBatch blit here. Omitted in this scaffold; FH5's validated backbuffer is 8-bit.
    auto eye_texture = backbuffer.Get();

    // AFR eye selection. Use the VR per-present render eye (m_render_eye, advanced once per present in
    // VR::on_post_present) — this is ALWAYS driven, including at menus. The adapter's g_fh5_applied_eye
    // stamp is only set while the main camera is being injected (is_main), so keying the begin/end cadence
    // off it stalls submission whenever the gameplay camera isn't active (blank SimXR at menus). At
    // gameplay both agree (the producer applies the same per-present-latched eye), so this stays correct
    // for stereo while restoring reliable submission everywhere.
    const int applied_eye = (int)vr->get_current_render_eye();   // 0=LEFT, 1=RIGHT

    // Begin the XR frame on the LEFT eye (first of the pair) so both copies land in a begun frame.
    if (applied_eye == 0 && !vr->m_openxr->frame_began) {
        vr->m_openxr->begin_frame();
    }

    m_openxr.copy(vr, applied_eye, eye_texture, device, command_queue);

    bool submitted = false;

    // On the RIGHT eye (second of the pair), begin (if still needed) + end the XR frame: both eyes are in.
    if (applied_eye == 1) {
        if (runtime->custom_stage == VRRuntime::SynchronizeStage::VERY_LATE) {
            runtime->synchronize_frame();
            if (!runtime->got_first_poses) {
                runtime->update_poses();
            }
        }

        if (!vr->m_openxr->frame_began) {
            vr->m_openxr->begin_frame();
        }

        auto result = vr->m_openxr->end_frame();

        if (result == XR_ERROR_LAYER_INVALID) {
            spdlog::info("[VR] Correcting invalid layer; waiting for all copies then retrying xrEndFrame");
            m_openxr.wait_for_all_copies();
            result = vr->m_openxr->end_frame();
        }

        vr->m_openxr->needs_pose_update = true;
        submitted = (result == XR_SUCCESS);
        vr->m_submitted = submitted;
    }

    m_prev_backbuffer = backbuffer;
    return submitted;
}

void D3D12Component::on_post_present(VR* vr) {
    // Nothing required for the OpenXR path (kept for parity with REFramework's surface).
}

void D3D12Component::on_reset(VR* vr) {
    for (auto& copier : m_generic_copiers) {
        copier.reset();
    }

    m_prev_backbuffer.Reset();

    auto runtime = vr->get_runtime();
    if (runtime != nullptr && runtime->is_openxr() && runtime->loaded) {
        // Recreate swapchains if the HMD render size changed.
        if (m_openxr.last_resolution[0] != vr->get_hmd_width() ||
            m_openxr.last_resolution[1] != vr->get_hmd_height() ||
            m_openxr.contexts.empty()) {
            m_openxr.create_swapchains(vr);
        }
    }
}

void D3D12Component::setup() {
    spdlog::info("[VR] Setting up D3D12 stereo component");

    m_prev_backbuffer.Reset();

    auto& hook = g_framework->get_d3d12_hook();
    auto device = hook->get_device();
    auto swapchain = hook->get_swap_chain();
    if (device == nullptr || swapchain == nullptr) {
        return;
    }

    ComPtr<ID3D12Resource> backbuffer{};
    if (FAILED(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))) || backbuffer == nullptr) {
        spdlog::error("[VR] setup: failed to get back buffer");
        return;
    }

    const auto desc = backbuffer->GetDesc();
    m_backbuffer_format = desc.Format;
    m_backbuffer_size[0] = (uint32_t)desc.Width;
    m_backbuffer_size[1] = (uint32_t)desc.Height;

    for (auto& copier : m_generic_copiers) {
        copier.setup(device, L"vrframework generic copier");
    }

    spdlog::info("[VR] D3D12 backbuffer {}x{} format {}", desc.Width, desc.Height, (int)desc.Format);
    m_force_reset = false;
}

// ---------------------------------------------------------------------------
// OpenXR swapchains
// ---------------------------------------------------------------------------
std::optional<std::string> D3D12Component::OpenXR::create_swapchains(VR* vr) {
    std::scoped_lock _{this->mtx};

    spdlog::info("[VR] Creating OpenXR swapchains for D3D12");

    destroy_swapchains(vr);

    auto& hook = g_framework->get_d3d12_hook();
    auto device = hook->get_device();
    auto swapchain = hook->get_swap_chain();
    if (device == nullptr || swapchain == nullptr) {
        return "No D3D12 device/swapchain";
    }

    auto& openxr = vr->m_openxr;
    if (openxr == nullptr || openxr->views.empty()) {
        return "OpenXR runtime not ready for swapchain creation";
    }

    this->contexts.clear();
    this->contexts.resize(openxr->views.size());
    openxr->swapchains.clear();

    const uint32_t width = vr->get_hmd_width();
    const uint32_t height = vr->get_hmd_height();

    for (size_t i = 0; i < openxr->views.size(); ++i) {
        spdlog::info("[VR] Creating swapchain for eye {} ({}x{})", i, width, height);

        XrSwapchainCreateInfo swapchain_create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchain_create_info.arraySize = 1;
        // Match the LIVE engine backbuffer format so CopyResource is valid (same TYPELESS family) and the
        // compositor interprets the bits the same way the game's display does. FH5 renders GAMEPLAY in HDR
        // R10G10B10A2 (format 24) and menus/showroom in R8G8B8A8 (28); a hardcoded 8-bit _SRGB swapchain
        // mismatches the 10-bit HDR backbuffer -> wrong color (purple). For 8-bit sRGB-encoded display data
        // we pick the _SRGB variant so the compositor's SRV decodes sRGB->linear correctly (SimXR L3159).
        // (NOTE: R10G10B10A2 has no _SRGB DXGI variant; SimXR's preview can't sRGB-decode 10-bit, so its
        // "O" window may still look off for HDR gameplay even though the swapchain content + a real HMD
        // compositor are correct. Full fix would tonemap/convert HDR->8bit sRGB in the copy shader.)
        DXGI_FORMAT bb_fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        {
            ComPtr<ID3D12Resource> bb{};
            if (SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&bb))) && bb != nullptr) {
                bb_fmt = bb->GetDesc().Format;
            }
        }
        DXGI_FORMAT xr_fmt;
        switch (bb_fmt) {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  xr_fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  xr_fmt = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
            case DXGI_FORMAT_R10G10B10A2_UNORM:    xr_fmt = DXGI_FORMAT_R10G10B10A2_UNORM;   break;  // HDR gameplay
            default:                               xr_fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
        }
        swapchain_create_info.format = xr_fmt;
        spdlog::info("[VR] eye swapchain format {} (backbuffer {})", (int)xr_fmt, (int)bb_fmt);
        swapchain_create_info.width = width;
        swapchain_create_info.height = height;
        swapchain_create_info.mipCount = 1;
        swapchain_create_info.faceCount = 1;
        swapchain_create_info.sampleCount = 1;
        swapchain_create_info.usageFlags =
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

        runtimes::OpenXR::Swapchain sc{};
        sc.width = (int32_t)width;
        sc.height = (int32_t)height;

        if (xrCreateSwapchain(openxr->session, &swapchain_create_info, &sc.handle) != XR_SUCCESS) {
            spdlog::error("[VR] Failed to create swapchain for eye {}", i);
            return "Failed to create swapchain";
        }

        openxr->swapchains.push_back(sc);

        uint32_t image_count{};
        if (xrEnumerateSwapchainImages(sc.handle, 0, &image_count, nullptr) != XR_SUCCESS) {
            return "Failed to enumerate swapchain images";
        }

        auto& ctx = this->contexts[i];
        ctx.textures.assign(image_count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
        ctx.copiers.clear();
        ctx.copiers.resize(image_count);
        for (uint32_t j = 0; j < image_count; ++j) {
            ctx.copiers[j] = std::make_unique<ResourceCopier>();
            ctx.copiers[j]->setup(device, L"OpenXR swapchain copier");
        }

        if (xrEnumerateSwapchainImages(sc.handle, image_count, &image_count,
                (XrSwapchainImageBaseHeader*)ctx.textures.data()) != XR_SUCCESS) {
            return "Failed to enumerate swapchain images (2)";
        }

        spdlog::info("[VR] Eye {} swapchain: {} images", i, image_count);
    }

    this->last_resolution = {width, height};
    return std::nullopt;
}

void D3D12Component::OpenXR::destroy_swapchains(VR* vr) {
    std::scoped_lock _{this->mtx};

    if (this->contexts.empty()) {
        return;
    }

    spdlog::info("[VR] Destroying OpenXR swapchains");

    auto& openxr = vr->m_openxr;
    for (size_t i = 0; i < this->contexts.size(); ++i) {
        this->contexts[i].copiers.clear();

        if (openxr != nullptr && i < openxr->swapchains.size()) {
            if (xrDestroySwapchain(openxr->swapchains[i].handle) != XR_SUCCESS) {
                spdlog::error("[VR] Failed to destroy swapchain {}", i);
            }
        }
        this->contexts[i].textures.clear();
    }

    this->contexts.clear();
    if (openxr != nullptr) {
        openxr->swapchains.clear();
    }
}

void D3D12Component::OpenXR::wait_for_all_copies() {
    std::scoped_lock _{this->mtx};
    for (auto& ctx : this->contexts) {
        for (auto& copier : ctx.copiers) {
            copier->wait(INFINITE);
        }
    }
}

void D3D12Component::OpenXR::copy(VR* vr, uint32_t swapchain_idx, ID3D12Resource* src,
                                  ID3D12Device* device, ID3D12CommandQueue* queue) {
    std::scoped_lock _{this->mtx};

    auto& openxr = vr->m_openxr;
    if (openxr == nullptr || swapchain_idx >= this->contexts.size() || swapchain_idx >= openxr->swapchains.size()) {
        return;
    }

    if (openxr->frame_state.shouldRender != XR_TRUE) {
        return;
    }

    if (!openxr->frame_began && openxr->custom_stage != VRRuntime::SynchronizeStage::VERY_LATE) {
        spdlog::error("[VR] OpenXR: frame not begun when trying to copy eye {}", swapchain_idx);
        return;
    }

    const auto& swapchain = openxr->swapchains[swapchain_idx];
    auto& ctx = this->contexts[swapchain_idx];

    // Acquire.
    uint32_t texture_index{};
    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    auto result = xrAcquireSwapchainImage(swapchain.handle, &acquire_info, &texture_index);

    if (result == XR_ERROR_RUNTIME_FAILURE) {
        spdlog::error("[VR] xrAcquireSwapchainImage failed: {}; attempting recovery", openxr->get_result_string(result));
        for (auto& copier : ctx.copiers) {
            copier->reset();
        }
        texture_index = 0;
        result = xrAcquireSwapchainImage(swapchain.handle, &acquire_info, &texture_index);
    }

    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrAcquireSwapchainImage failed: {}", openxr->get_result_string(result));
        return;
    }

    ctx.num_textures_acquired++;

    // Wait.
    XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait_info.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(swapchain.handle, &wait_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrWaitSwapchainImage failed: {}", openxr->get_result_string(result));
        return;
    }

    // Copy src (engine backbuffer, PRESENT) -> runtime image (RENDER_TARGET).
    auto& copier = ctx.copiers[texture_index];
    copier->wait(INFINITE);
    copier->copy(src, ctx.textures[texture_index].texture,
                 D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    copier->execute(queue);

    // Release.
    XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(swapchain.handle, &release_info);

    if (result == XR_ERROR_RUNTIME_FAILURE) {
        // SteamVR shenanigans (per REFramework): re-wait, drain copies, retry release.
        spdlog::error("[VR] xrReleaseSwapchainImage failed: {}; attempting recovery", openxr->get_result_string(result));
        xrWaitSwapchainImage(swapchain.handle, &wait_info);
        for (auto& c : ctx.copiers) {
            c->wait(INFINITE);
        }
        result = xrReleaseSwapchainImage(swapchain.handle, &release_info);
    }

    if (result != XR_SUCCESS) {
        spdlog::error("[VR] xrReleaseSwapchainImage failed: {}", openxr->get_result_string(result));
        return;
    }

    ctx.num_textures_acquired--;
}

} // namespace vrmod
