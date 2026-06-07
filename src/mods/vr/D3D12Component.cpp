// PORT FROM: REFramework/src/mods/vr/D3D12Component.cpp (OpenXR D3D12 path only).
// See header for the simplification rationale. The acquire/wait/copy/release + xrEndFrame
// sequence here is the one validated in FH5CameraProbe/src/XrSimTest.cpp.

#include <spdlog/spdlog.h>

#include "Framework.hpp"
#include "mods/VR.hpp"
#include "mods/vr/D3D12Component.hpp"
#include "../../fh5vr/Fh5Adapter.hpp"
#include "../../fh5vr/Fh5CameraCbuffer.hpp"

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
                          D3D12_RESOURCE_STATES src_state, D3D12_RESOURCE_STATES dst_state, int x_shift) {
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
    if (x_shift == 0) {
        cmd_list->CopyResource(dst, src);
    } else {
        // DIAGNOSTIC: copy the image shifted right by x_shift px (src columns [0..W-x_shift) -> dst [x_shift..W)).
        const auto sd = src->GetDesc();
        const UINT w = (UINT)sd.Width, h = sd.Height;
        const UINT sx = (x_shift > 0) ? 0u : (UINT)(-x_shift);
        const UINT dx = (x_shift > 0) ? (UINT)x_shift : 0u;
        const UINT cw = (w > (UINT)(x_shift > 0 ? x_shift : -x_shift)) ? w - (UINT)(x_shift > 0 ? x_shift : -x_shift) : 0u;
        D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = dst; d.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; d.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = src; s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
        D3D12_BOX box{}; box.left = sx; box.top = 0; box.front = 0; box.right = sx + cw; box.bottom = h; box.back = 1;
        cmd_list->CopyTextureRegion(&d, dx, 0, 0, &s, &box);
    }

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

    // Eye selection has two modes:
    //   * gameplay: use the fresh producer stamp, because it names the eye the engine camera actually
    //     rendered. This prevents AER phase drift when menus/reset reorder present vs producer timing.
    //   * menus/loading: fall back to AFR parity so the OpenXR preview keeps receiving frames even when
    //     the gameplay-camera producer is not active.
    const uint64_t now_for_eye_ms = ::GetTickCount64();
    const uint64_t applied_eye_ms = fh5diag::applied_eye_ms();
    const bool producer_eye_fresh = applied_eye_ms != 0 && (now_for_eye_ms - applied_eye_ms) <= 250;
    const int applied_eye = producer_eye_fresh
        ? fh5diag::applied_eye()
        : (int)vr->get_current_render_eye();   // 0=LEFT, 1=RIGHT

    // Diagnostic: log densely during startup/reset, then at low rate. This proves whether the producer eye,
    // copy eye, backbuffer index, and XR begin/end state are paired instead of only sampling one line/5s.
    uint32_t copy_seq = 0;
    {
        static std::atomic<uint32_t> s_seq{ 0 };
        static std::atomic<uint64_t> s_last_log_ms{ 0 };
        copy_seq = s_seq.fetch_add(1, std::memory_order_relaxed);
        const uint64_t now_ms = ::GetTickCount64();
        uint64_t last_ms = s_last_log_ms.load(std::memory_order_relaxed);
        if ((copy_seq < 120 || now_ms - last_ms >= 1000) &&
            s_last_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
            spdlog::info("[VR-COPY] present#{} eye={} src={} bbIdx={} began={} synced={} shouldRender={}",
                copy_seq, applied_eye, producer_eye_fresh ? "producer" : "parity",
                backbuffer_index,
                vr->m_openxr->frame_began ? 1 : 0,
                vr->m_openxr->frame_synced ? 1 : 0,
                vr->m_openxr->frame_state.shouldRender == XR_TRUE ? 1 : 0);
        }
    }

    // Begin the XR frame on the LEFT eye (first of the pair) so both copies land in a begun frame.
    if (applied_eye == 0 && !vr->m_openxr->frame_began) {
        if (!vr->m_openxr->frame_synced) {
            runtime->synchronize_frame();
            runtime->update_poses();
        }
        vr->m_openxr->begin_frame();
    }

    if (applied_eye == 1 && !vr->m_openxr->frame_began) {
        static std::atomic<uint64_t> s_last_drop_log_ms{ 0 };
        const uint64_t now_ms = ::GetTickCount64();
        uint64_t last_ms = s_last_drop_log_ms.load(std::memory_order_relaxed);
        if (now_ms - last_ms >= 1000 &&
            s_last_drop_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
            spdlog::warn("[VR-COPY] drop unpaired right eye present#{} bbIdx={} (no begun XR frame)", copy_seq, backbuffer_index);
        }
        return false;
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

        // UI/HUD quad layer (gated by hudquad=on). Copy the chosen UI source into the HUD swapchain and
        // submit it as a head-locked quad in view_space alongside the eye projection. INITIAL source = the
        // engine backbuffer (proves swapchain/copy/quad/compositing end-to-end with a recognizable image);
        // this is replaced by the UI-only texture once FH5's UI render target is identified. hud_quad must
        // outlive the end_frame call below, so it is declared here in the submit scope.
        std::vector<XrCompositionLayerBaseHeader*> extra_layers{};
        XrCompositionLayerQuad hud_quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        if (fh5cb::ctl_hud_quad() && m_openxr.copy_hud(vr, eye_texture, device, command_queue)) {
            const float aspect = (m_openxr.hud_height > 0)
                ? (float)m_openxr.hud_width / (float)m_openxr.hud_height : (16.0f / 9.0f);
            const float quad_w = fh5cb::ctl_hud_w();        // metres wide (live-tunable)
            const float quad_h = quad_w / aspect;           // aspect-correct height
            // Opaque during backbuffer validation (the backbuffer alpha may be 0 -> a source-alpha quad would
            // be fully transparent/invisible). Switch to source-alpha once the quad source is a real UI texture.
            hud_quad.layerFlags = fh5cb::ctl_hud_opaque() ? 0 : XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            hud_quad.space = vr->m_openxr->view_space;       // head-locked panel
            hud_quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            hud_quad.subImage.swapchain = m_openxr.hud_handle;
            hud_quad.subImage.imageArrayIndex = 0;
            hud_quad.subImage.imageRect.offset = {0, 0};
            hud_quad.subImage.imageRect.extent = {m_openxr.hud_width, m_openxr.hud_height};
            hud_quad.pose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
            hud_quad.pose.position = {fh5cb::ctl_hud_x(), fh5cb::ctl_hud_y(), fh5cb::ctl_hud_z()};
            hud_quad.size = {quad_w, quad_h};
            extra_layers.push_back((XrCompositionLayerBaseHeader*)&hud_quad);
            static std::atomic<uint64_t> s_hud_log_ms{0};
            const uint64_t now_ms = ::GetTickCount64();
            uint64_t last_ms = s_hud_log_ms.load(std::memory_order_relaxed);
            if (now_ms - last_ms >= 2000 && s_hud_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                spdlog::info("[VR-HUDQUAD] quad {:.2f}x{:.2f}m pos=({:.2f},{:.2f},{:.2f}) opaque={} (swapchain {}x{})",
                    quad_w, quad_h, fh5cb::ctl_hud_x(), fh5cb::ctl_hud_y(), fh5cb::ctl_hud_z(),
                    fh5cb::ctl_hud_opaque() ? 1 : 0, m_openxr.hud_width, m_openxr.hud_height);
            }
        }

        auto result = vr->m_openxr->end_frame(extra_layers);

        if (result == XR_ERROR_LAYER_INVALID) {
            spdlog::info("[VR] Correcting invalid layer; waiting for all copies then retrying xrEndFrame (eyes only)");
            m_openxr.wait_for_all_copies();
            result = vr->m_openxr->end_frame();   // retry without the quad in case it was the invalid layer
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
        if (vr->m_openxr != nullptr) {
            std::scoped_lock _{vr->m_openxr->sync_mtx};
            vr->m_openxr->frame_began = false;
            vr->m_openxr->frame_synced = false;
            vr->m_openxr->needs_pose_update = true;
        }

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

    // Size the eye swapchains to the LIVE BACKBUFFER, not the HMD's ideal per-eye size. The copy is a 1:1
    // CopyResource (no scaling), so a swapchain that doesn't match the backbuffer dimensions makes every
    // copy fail -> DEVICE_REMOVED crash (FH5 renders at e.g. 1600x843, not 1280x720). The OpenXR compositor
    // scales whatever swapchain size we submit to the eye, so matching the backbuffer is both safe and
    // correct. Fall back to the HMD size if the backbuffer can't be read.
    uint32_t width = vr->get_hmd_width();
    uint32_t height = vr->get_hmd_height();
    DXGI_FORMAT bb_fmt_probe = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    {
        ComPtr<ID3D12Resource> bb0{};
        if (SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&bb0))) && bb0 != nullptr) {
            const auto d = bb0->GetDesc();
            if (d.Width > 0 && d.Height > 0) { width = (uint32_t)d.Width; height = d.Height; }
            bb_fmt_probe = d.Format;
        }
    }

    for (size_t i = 0; i < openxr->views.size(); ++i) {
        spdlog::info("[VR] Creating swapchain for eye {} ({}x{}) [backbuffer-sized]", i, width, height);

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
        DXGI_FORMAT bb_fmt = bb_fmt_probe;
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

    // UI/HUD quad swapchain (separate from the eye set). RGBA8 sRGB so it carries alpha for the UI and the
    // compositor decodes sRGB correctly; sized to the backbuffer for a 1:1 CopyResource (the initial quad
    // source is the engine backbuffer to validate the path, later the UI-only texture). Non-fatal on failure.
    if (auto hud_err = create_hud_swapchain(vr, device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)) {
        spdlog::warn("[VR] HUD quad swapchain not created: {} (eye rendering unaffected)", *hud_err);
    }

    return std::nullopt;
}

std::optional<std::string> D3D12Component::OpenXR::create_hud_swapchain(VR* vr, ID3D12Device* device,
                                                                       uint32_t width, uint32_t height,
                                                                       DXGI_FORMAT xr_fmt) {
    std::scoped_lock _{this->mtx};
    this->hud_ready = false;

    auto& openxr = vr->m_openxr;
    if (openxr == nullptr || device == nullptr) {
        return "no device/runtime";
    }

    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.arraySize = 1;
    ci.format = xr_fmt;
    ci.width = width;
    ci.height = height;
    ci.mipCount = 1;
    ci.faceCount = 1;
    ci.sampleCount = 1;
    ci.usageFlags =
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;

    if (xrCreateSwapchain(openxr->session, &ci, &this->hud_handle) != XR_SUCCESS) {
        this->hud_handle = XR_NULL_HANDLE;
        return "xrCreateSwapchain (HUD) failed";
    }
    this->hud_width = (int32_t)width;
    this->hud_height = (int32_t)height;

    uint32_t image_count{};
    if (xrEnumerateSwapchainImages(this->hud_handle, 0, &image_count, nullptr) != XR_SUCCESS) {
        return "enumerate HUD images failed";
    }
    this->hud_ctx.textures.assign(image_count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
    this->hud_ctx.copiers.clear();
    this->hud_ctx.copiers.resize(image_count);
    for (uint32_t j = 0; j < image_count; ++j) {
        this->hud_ctx.copiers[j] = std::make_unique<ResourceCopier>();
        this->hud_ctx.copiers[j]->setup(device, L"OpenXR HUD quad copier");
    }
    if (xrEnumerateSwapchainImages(this->hud_handle, image_count, &image_count,
            (XrSwapchainImageBaseHeader*)this->hud_ctx.textures.data()) != XR_SUCCESS) {
        return "enumerate HUD images (2) failed";
    }

    this->hud_ready = true;
    spdlog::info("[VR] HUD quad swapchain created ({}x{}, {} images, fmt {})", width, height, image_count, (int)xr_fmt);
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

    // HUD quad swapchain.
    this->hud_ready = false;
    this->hud_ctx.copiers.clear();
    this->hud_ctx.textures.clear();
    if (this->hud_handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(this->hud_handle);
        this->hud_handle = XR_NULL_HANDLE;
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
                                  ID3D12Device* device, ID3D12CommandQueue* queue, int x_shift) {
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
                 D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, x_shift);
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

bool D3D12Component::OpenXR::copy_hud(VR* vr, ID3D12Resource* src, ID3D12Device* device, ID3D12CommandQueue* queue) {
    std::scoped_lock _{this->mtx};

    auto& openxr = vr->m_openxr;
    if (!this->hud_ready || this->hud_handle == XR_NULL_HANDLE || openxr == nullptr || src == nullptr) {
        return false;
    }
    if (openxr->frame_state.shouldRender != XR_TRUE || !openxr->frame_began) {
        return false;
    }

    uint32_t texture_index{};
    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    auto result = xrAcquireSwapchainImage(this->hud_handle, &acquire_info, &texture_index);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD xrAcquireSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }

    XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait_info.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(this->hud_handle, &wait_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD xrWaitSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }

    auto& copier = this->hud_ctx.copiers[texture_index];
    copier->wait(INFINITE);
    // src is the engine backbuffer (PRESENT) for the initial path-validation; later this is the UI texture.
    copier->copy(src, this->hud_ctx.textures[texture_index].texture,
                 D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    copier->execute(queue);

    XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(this->hud_handle, &release_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD xrReleaseSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }
    return true;
}

} // namespace vrmod
