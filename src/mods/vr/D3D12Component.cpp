// PORT FROM: REFramework/src/mods/vr/D3D12Component.cpp (OpenXR D3D12 path only).
// See header for the simplification rationale. The acquire/wait/copy/release + xrEndFrame
// sequence here is the one validated in FH5CameraProbe/src/XrSimTest.cpp.

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <d3dcompiler.h>

#include "Framework.hpp"
#include "mods/VR.hpp"
#include "mods/vr/D3D12Component.hpp"
#include "../../fh5vr/Fh5Adapter.hpp"
#include "../../fh5vr/Fh5CameraCbuffer.hpp"

namespace vrmod {

namespace {

DXGI_FORMAT CopyFormatFamily(DXGI_FORMAT fmt) {
    switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    default:
        return fmt;
    }
}

bool CopyResourceCompatible(const D3D12_RESOURCE_DESC& src, const D3D12_RESOURCE_DESC& dst) {
    return src.Dimension == dst.Dimension &&
           src.Width == dst.Width &&
           src.Height == dst.Height &&
           src.DepthOrArraySize == dst.DepthOrArraySize &&
           src.MipLevels == dst.MipLevels &&
           src.SampleDesc.Count == dst.SampleDesc.Count &&
           src.SampleDesc.Quality == dst.SampleDesc.Quality &&
           CopyFormatFamily(src.Format) == CopyFormatFamily(dst.Format);
}

DXGI_FORMAT SrvReadableFormat(DXGI_FORMAT fmt) {
    switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    default:
        return fmt;
    }
}

void LogShaderBlob(const char* label, ID3DBlob* blob) {
    if (blob == nullptr || blob->GetBufferPointer() == nullptr || blob->GetBufferSize() == 0) {
        return;
    }
    const size_t n = std::min<size_t>(blob->GetBufferSize(), 768);
    std::string msg(static_cast<const char*>(blob->GetBufferPointer()), n);
    spdlog::error("[VR-HUDBLIT] {}: {}", label, msg);
}

bool FiniteVector(const XrVector3f& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool FiniteQuaternion(const XrQuaternionf& q) {
    return std::isfinite(q.x) && std::isfinite(q.y) &&
           std::isfinite(q.z) && std::isfinite(q.w);
}

bool ValidSpacePose(const XrSpaceLocation& loc) {
    constexpr XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    return (loc.locationFlags & required) == required &&
           FiniteVector(loc.pose.position) &&
           FiniteQuaternion(loc.pose.orientation);
}

XrVector3f Cross(const XrVector3f& a, const XrVector3f& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

XrVector3f RotateVector(const XrQuaternionf& q, const XrVector3f& v) {
    const XrVector3f u{q.x, q.y, q.z};
    const XrVector3f uv = Cross(u, v);
    const XrVector3f uuv = Cross(u, uv);
    return {
        v.x + 2.0f * (q.w * uv.x + uuv.x),
        v.y + 2.0f * (q.w * uv.y + uuv.y),
        v.z + 2.0f * (q.w * uv.z + uuv.z)
    };
}

XrQuaternionf NormalizeQuaternion(XrQuaternionf q) {
    const float len_sq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (!std::isfinite(len_sq) || len_sq <= 0.000001f) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    const float inv_len = 1.0f / std::sqrt(len_sq);
    q.x *= inv_len;
    q.y *= inv_len;
    q.z *= inv_len;
    q.w *= inv_len;
    return q;
}

XrQuaternionf BuildYawOnlyAnchor(const XrQuaternionf& head_orientation) {
    const XrQuaternionf head_q = NormalizeQuaternion(head_orientation);
    XrVector3f forward = RotateVector(head_q, {0.0f, 0.0f, -1.0f});
    const float len = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    if (!std::isfinite(len) || len <= 0.001f) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    forward.x /= len;
    forward.z /= len;
    const float yaw = std::atan2(-forward.x, -forward.z);
    return NormalizeQuaternion({0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)});
}

XrPosef HudPoseFromAnchor(const XrVector3f& anchor_origin,
                          const XrQuaternionf& anchor_orientation,
                          const XrVector3f& local_offset) {
    const XrVector3f world_offset = RotateVector(anchor_orientation, local_offset);
    XrPosef pose{anchor_orientation, {
        anchor_origin.x + world_offset.x,
        anchor_origin.y + world_offset.y,
        anchor_origin.z + world_offset.z
    }};
    return pose;
}

bool ReadyToLockHudAnchor() {
    // The OpenXR simulator reports a default LOCAL pose during FH5 splash/loading, then
    // jumps to the real seated pose once the game camera is active. Locking the quad
    // before that makes a fixed panel land above/outside the user's later view.
    return fh5cb::cam_hits() > 0;
}

} // namespace

// ===========================================================================
// HudBlitter - fullscreen triangle scaler/compositor for HUD quad sources
// ===========================================================================
struct HudBlitter {
    template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    ~HudBlitter() { reset(); }

    bool setup(ID3D12Device* device, DXGI_FORMAT rt_format) {
        if (device == nullptr || rt_format == DXGI_FORMAT_UNKNOWN) {
            return false;
        }
        if (pso != nullptr && cached_device.Get() == device && cached_rt_format == rt_format) {
            return true;
        }

        reset();
        cached_device = device;
        cached_rt_format = rt_format;

        HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateCommandAllocator failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }
        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmd_list));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateCommandList failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }
        cmd_list->SetName(L"OpenXR HUD quad blitter");
        cmd_list->Close();

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateFence failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }
        fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (fence_event == nullptr) {
            spdlog::error("[VR-HUDBLIT] CreateEvent failed");
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
        srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srv_heap_desc.NumDescriptors = 2;
        srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&srv_heap));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateDescriptorHeap(SRV) failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.NumDescriptors = 1;
        hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateDescriptorHeap(RTV) failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        static constexpr const char* kHlsl = R"(
Texture2D g_src : register(t0);
Texture2D g_base : register(t1);
SamplerState g_samp : register(s0);
cbuffer FlipCB : register(b0) { uint g_flipv; uint3 _flippad; };

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float2 FlipUV(float2 uv) { return (g_flipv != 0) ? float2(uv.x, 1.0 - uv.y) : uv; }

VSOut VSMain(uint id : SV_VertexID) {
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    float2 uv[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };
    VSOut o;
    o.pos = float4(pos[id], 0.0, 1.0);
    o.uv = uv[id];
    return o;
}

float4 PSMain(VSOut i) : SV_Target {
    float2 uv = FlipUV(i.uv);
    float4 c = g_src.SampleLevel(g_samp, uv, 0.0);
    float alpha = saturate((max(max(c.r, c.g), c.b) - 0.01) * 12.0);
    return float4(c.rgb, alpha);
}

float4 PSDiff(VSOut i) : SV_Target {
    float2 uv = FlipUV(i.uv);
    float4 final_color = g_src.SampleLevel(g_samp, uv, 0.0);
    float4 base_color = g_base.SampleLevel(g_samp, uv, 0.0);
    float3 delta = abs(final_color.rgb - base_color.rgb);
    float mask = saturate((max(max(delta.r, delta.g), delta.b) - 0.003) * 100.0);
    float3 rgb = saturate(final_color.rgb * mask + delta * 3.0);
    float visible = max(max(rgb.r, rgb.g), rgb.b);
    float alpha = saturate(max(mask * 3.0, (visible - 0.01) * 4.0));
    return float4(rgb, alpha);
}
)";

        ComPtr<ID3DBlob> vs{};
        ComPtr<ID3DBlob> ps{};
        ComPtr<ID3DBlob> ps_diff{};
        ComPtr<ID3DBlob> err{};
        UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
        hr = D3DCompile(kHlsl, std::strlen(kHlsl), "HudQuadBlit", nullptr, nullptr,
                        "VSMain", "vs_5_0", flags, 0, &vs, &err);
        if (FAILED(hr)) {
            LogShaderBlob("VS compile failed", err.Get());
            return false;
        }
        err.Reset();
        hr = D3DCompile(kHlsl, std::strlen(kHlsl), "HudQuadBlit", nullptr, nullptr,
                        "PSMain", "ps_5_0", flags, 0, &ps, &err);
        if (FAILED(hr)) {
            LogShaderBlob("PS compile failed", err.Get());
            return false;
        }
        err.Reset();
        hr = D3DCompile(kHlsl, std::strlen(kHlsl), "HudQuadBlit", nullptr, nullptr,
                        "PSDiff", "ps_5_0", flags, 0, &ps_diff, &err);
        if (FAILED(hr)) {
            LogShaderBlob("PSDiff compile failed", err.Get());
            return false;
        }

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 2;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER root_params[2]{};
        root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_params[0].DescriptorTable.NumDescriptorRanges = 1;
        root_params[0].DescriptorTable.pDescriptorRanges = &range;
        root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        // b0: 1-DWORD flip flag (g_flipv) so the quad V-orientation is runtime-toggleable (hudflipv=on/off)
        // without a hardcoded flip that would be wrong on a runtime with the opposite quad-layer V convention.
        root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_params[1].Constants.ShaderRegister = 0;
        root_params[1].Constants.RegisterSpace = 0;
        root_params[1].Constants.Num32BitValues = 1;
        root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rs_desc{};
        rs_desc.NumParameters = 2;
        rs_desc.pParameters = root_params;
        rs_desc.NumStaticSamplers = 1;
        rs_desc.pStaticSamplers = &sampler;
        rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> sig{};
        err.Reset();
        hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
        if (FAILED(hr)) {
            LogShaderBlob("root signature serialize failed", err.Get());
            return false;
        }
        hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&root_sig));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateRootSignature failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
        pso_desc.pRootSignature = root_sig.Get();
        pso_desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        pso_desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
        pso_desc.BlendState.IndependentBlendEnable = FALSE;
        auto& blend = pso_desc.BlendState.RenderTarget[0];
        blend.BlendEnable = FALSE;
        blend.LogicOpEnable = FALSE;
        blend.SrcBlend = D3D12_BLEND_ONE;
        blend.DestBlend = D3D12_BLEND_ZERO;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
        pso_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pso_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pso_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pso_desc.RasterizerState.DepthClipEnable = TRUE;
        pso_desc.RasterizerState.MultisampleEnable = FALSE;
        pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
        pso_desc.RasterizerState.ForcedSampleCount = 0;
        pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.InputLayout = {nullptr, 0};
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = rt_format;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.SampleDesc.Quality = 0;

        hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] CreateGraphicsPipelineState failed: 0x{:08X} rt_fmt={}", (uint32_t)hr, (int)rt_format);
            return false;
        }
        pso_desc.PS = {ps_diff->GetBufferPointer(), ps_diff->GetBufferSize()};
        hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_diff));
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDDIFF] CreateGraphicsPipelineState failed: 0x{:08X} rt_fmt={}", (uint32_t)hr, (int)rt_format);
            return false;
        }

        spdlog::info("[VR-HUDBLIT] initialized fullscreen HUD blitter rt_fmt={}", (int)rt_format);
        return true;
    }

    void reset() {
        if (fence_event != nullptr) {
            wait(INFINITE);
            CloseHandle(fence_event);
            fence_event = nullptr;
        }
        pso.Reset();
        pso_diff.Reset();
        root_sig.Reset();
        srv_heap.Reset();
        rtv_heap.Reset();
        allocator.Reset();
        cmd_list.Reset();
        fence.Reset();
        cached_device.Reset();
        cached_rt_format = DXGI_FORMAT_UNKNOWN;
        fence_value = 0;
        waiting = false;
    }

    void wait(uint32_t ms) {
        if (fence == nullptr || fence_event == nullptr || !waiting) {
            return;
        }
        if (fence->GetCompletedValue() < fence_value) {
            fence->SetEventOnCompletion(fence_value, fence_event);
            WaitForSingleObject(fence_event, ms);
        }
        waiting = false;
    }

    struct DumpStats {
        uint64_t nonzero_bytes{0};
        uint8_t max_byte{0};
    };

    static float decode_ufloat(uint32_t bits, int mantissa_bits) {
        const uint32_t mantissa_mask = (1u << mantissa_bits) - 1u;
        const uint32_t mantissa = bits & mantissa_mask;
        const uint32_t exponent = bits >> mantissa_bits;
        if (exponent == 0) {
            return mantissa ? std::ldexp(static_cast<float>(mantissa), -14 - mantissa_bits) : 0.0f;
        }
        if (exponent == 31) {
            return 1.0f;
        }
        return std::ldexp(1.0f + static_cast<float>(mantissa) / static_cast<float>(1u << mantissa_bits),
                          static_cast<int>(exponent) - 15);
    }

    static uint8_t linear_to_u8(float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        return static_cast<uint8_t>(v * 255.0f + 0.5f);
    }

    static bool save_texture_bmp(const char* path, const uint8_t* data, uint32_t width, uint32_t height,
                                 uint32_t row_pitch, DXGI_FORMAT format, DumpStats* stats = nullptr) {
        if (path == nullptr || data == nullptr || width == 0 || height == 0) {
            return false;
        }

        FILE* fp = nullptr;
        if (fopen_s(&fp, path, "wb") != 0 || fp == nullptr) {
            return false;
        }

        const uint32_t out_pitch = (width * 3u + 3u) & ~3u;
        const uint32_t pixel_bytes = out_pitch * height;
        const uint32_t file_bytes = 14u + 40u + pixel_bytes;
        auto put_u16 = [fp](uint16_t v) {
            fputc(v & 0xFF, fp);
            fputc((v >> 8) & 0xFF, fp);
        };
        auto put_u32 = [fp](uint32_t v) {
            fputc(v & 0xFF, fp);
            fputc((v >> 8) & 0xFF, fp);
            fputc((v >> 16) & 0xFF, fp);
            fputc((v >> 24) & 0xFF, fp);
        };

        fputc('B', fp);
        fputc('M', fp);
        put_u32(file_bytes);
        put_u16(0);
        put_u16(0);
        put_u32(14u + 40u);
        put_u32(40u);
        put_u32(width);
        put_u32(height);
        put_u16(1);
        put_u16(24);
        put_u32(0);
        put_u32(pixel_bytes);
        put_u32(2835);
        put_u32(2835);
        put_u32(0);
        put_u32(0);

        std::vector<uint8_t> row(out_pitch);
        const bool bgra =
            format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        DumpStats local_stats{};
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* src_row = data + (height - 1u - y) * row_pitch;
            for (uint32_t x = 0; x < width; ++x) {
                const uint8_t* p = src_row + x * 4u;
                uint8_t r = 0, g = 0, b = 0;
                if (format == DXGI_FORMAT_R11G11B10_FLOAT) {
                    const uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
                    r = linear_to_u8(decode_ufloat(v & 0x7FFu, 6));
                    g = linear_to_u8(decode_ufloat((v >> 11) & 0x7FFu, 6));
                    b = linear_to_u8(decode_ufloat((v >> 22) & 0x3FFu, 5));
                } else if (format == DXGI_FORMAT_R10G10B10A2_UNORM) {
                    const uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
                    r = static_cast<uint8_t>(((v & 0x3FFu) * 255u + 511u) / 1023u);
                    g = static_cast<uint8_t>(((((v >> 10) & 0x3FFu) * 255u + 511u) / 1023u));
                    b = static_cast<uint8_t>(((((v >> 20) & 0x3FFu) * 255u + 511u) / 1023u));
                } else {
                    r = bgra ? p[2] : p[0];
                    g = p[1];
                    b = bgra ? p[0] : p[2];
                }
                row[x * 3u + 0u] = b;
                row[x * 3u + 1u] = g;
                row[x * 3u + 2u] = r;
                for (uint32_t c = 0; c < 4u; ++c) {
                    local_stats.max_byte = std::max(local_stats.max_byte, p[c]);
                    if (p[c] != 0) {
                        ++local_stats.nonzero_bytes;
                    }
                }
            }
            fwrite(row.data(), 1, out_pitch, fp);
        }

        fclose(fp);
        if (stats != nullptr) {
            *stats = local_stats;
        }
        return true;
    }

    bool blit(ID3D12Resource* src, ID3D12Resource* dst, ID3D12Device* device, ID3D12CommandQueue* queue,
              D3D12_RESOURCE_STATES src_state, D3D12_CPU_DESCRIPTOR_HANDLE src_srv = D3D12_CPU_DESCRIPTOR_HANDLE{}) {
        if (src == nullptr || dst == nullptr || device == nullptr || queue == nullptr) {
            return false;
        }

        const D3D12_RESOURCE_DESC src_desc = src->GetDesc();
        const D3D12_RESOURCE_DESC dst_desc = dst->GetDesc();
        if (src_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            dst_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            src_desc.SampleDesc.Count != 1 || dst_desc.SampleDesc.Count != 1) {
            spdlog::warn("[VR-HUDBLIT] unsupported src/dst desc src_dim={} src_samples={} dst_dim={} dst_samples={}",
                (int)src_desc.Dimension, (uint32_t)src_desc.SampleDesc.Count,
                (int)dst_desc.Dimension, (uint32_t)dst_desc.SampleDesc.Count);
            return false;
        }

        const bool has_src_srv = src_srv.ptr != 0;
        const DXGI_FORMAT srv_format = has_src_srv ? DXGI_FORMAT_UNKNOWN : SrvReadableFormat(src_desc.Format);
        if (!has_src_srv && srv_format == DXGI_FORMAT_UNKNOWN) {
            spdlog::warn("[VR-HUDBLIT] unsupported source format {}", (int)src_desc.Format);
            return false;
        }

        if (!setup(device, dst_desc.Format)) {
            return false;
        }

        wait(INFINITE);
        allocator->Reset();
        cmd_list->Reset(allocator.Get(), pso.Get());

        static std::atomic<uint32_t> s_dump_seq{0};
        const uint32_t dump_seq = s_dump_seq.fetch_add(1, std::memory_order_relaxed) + 1u;
        bool dump_this_frame = false;
        const bool dumpable_shader_source =
            src_desc.Width > 0 && src_desc.Height > 0 && src_desc.Width <= 4096 && src_desc.Height <= 4096;
        const bool dump_sample = dump_seq == 1u || dump_seq == 30u || dump_seq == 120u || dump_seq == 300u;
        if (dumpable_shader_source &&
            (dst_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || dst_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
             dst_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || dst_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
             dst_desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM) &&
            dump_sample) {
            dump_this_frame = true;
        }

        ComPtr<ID3D12Resource> dump_readback{};
        ComPtr<ID3D12Resource> dump_src_readback{};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT dump_footprint{};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT dump_src_footprint{};
        UINT dump_rows = 0;
        UINT dump_src_rows = 0;
        UINT64 dump_row_bytes = 0;
        UINT64 dump_src_row_bytes = 0;
        UINT64 dump_total_bytes = 0;
        UINT64 dump_src_total_bytes = 0;
        if (dump_this_frame) {
            auto make_readback = [device](const D3D12_RESOURCE_DESC& tex_desc, UINT64 bytes,
                                          ComPtr<ID3D12Resource>& out, const char* label) -> bool {
                D3D12_HEAP_PROPERTIES hp{};
                hp.Type = D3D12_HEAP_TYPE_READBACK;
                D3D12_RESOURCE_DESC rb_desc{};
                rb_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                rb_desc.Width = bytes;
                rb_desc.Height = 1;
                rb_desc.DepthOrArraySize = 1;
                rb_desc.MipLevels = 1;
                rb_desc.SampleDesc.Count = 1;
                rb_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                HRESULT rb_hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rb_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&out));
                if (FAILED(rb_hr) || out == nullptr) {
                    spdlog::warn("[VR-HUDBLIT] {} readback buffer create failed: 0x{:08X}", label, (uint32_t)rb_hr);
                    return false;
                }
                (void)tex_desc;
                return true;
            };

            device->GetCopyableFootprints(&dst_desc, 0, 1, 0, &dump_footprint, &dump_rows,
                &dump_row_bytes, &dump_total_bytes);
            device->GetCopyableFootprints(&src_desc, 0, 1, 0, &dump_src_footprint, &dump_src_rows,
                &dump_src_row_bytes, &dump_src_total_bytes);
            const bool dst_ok = make_readback(dst_desc, dump_total_bytes, dump_readback, "dst");
            const bool src_ok = make_readback(src_desc, dump_src_total_bytes, dump_src_readback, "src");
            dump_this_frame = dst_ok || src_ok;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = srv_format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = src_desc.MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.PlaneSlice = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
        D3D12_CPU_DESCRIPTOR_HANDLE srv0 = srv_heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE srv1 = srv0;
        srv1.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if (has_src_srv) {
            device->CopyDescriptorsSimple(1, srv0, src_srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            device->CopyDescriptorsSimple(1, srv1, src_srv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        } else {
            device->CreateShaderResourceView(src, &srv_desc, srv0);
            device->CreateShaderResourceView(src, &srv_desc, srv1);
        }
        device->CreateRenderTargetView(dst, nullptr, rtv_heap->GetCPUDescriptorHandleForHeapStart());

        const bool transition_src =
            (src_state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        D3D12_RESOURCE_BARRIER src_barrier{};
        if (transition_src) {
            src_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            src_barrier.Transition.pResource = src;
            src_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            src_barrier.Transition.StateBefore = src_state;
            src_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmd_list->ResourceBarrier(1, &src_barrier);
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        cmd_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        cmd_list->SetGraphicsRootSignature(root_sig.Get());
        ID3D12DescriptorHeap* heaps[] = {srv_heap.Get()};
        cmd_list->SetDescriptorHeaps(1, heaps);
        cmd_list->SetGraphicsRootDescriptorTable(0, srv_heap->GetGPUDescriptorHandleForHeapStart());
        cmd_list->SetGraphicsRoot32BitConstant(1, fh5cb::ctl_hud_flipv() ? 1u : 0u, 0);

        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)dst_desc.Width;
        viewport.Height = (float)dst_desc.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        D3D12_RECT scissor{0, 0, (LONG)dst_desc.Width, (LONG)dst_desc.Height};
        cmd_list->RSSetViewports(1, &viewport);
        cmd_list->RSSetScissorRects(1, &scissor);
        cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->DrawInstanced(3, 1, 0, 0);

        if (dump_this_frame && dump_readback != nullptr) {
            D3D12_RESOURCE_BARRIER dst_barrier{};
            dst_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            dst_barrier.Transition.pResource = dst;
            dst_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            dst_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            dst_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            cmd_list->ResourceBarrier(1, &dst_barrier);

            D3D12_TEXTURE_COPY_LOCATION src_loc{};
            src_loc.pResource = dst;
            src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_loc.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION dst_loc{};
            dst_loc.pResource = dump_readback.Get();
            dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst_loc.PlacedFootprint = dump_footprint;
            cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

            dst_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            dst_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmd_list->ResourceBarrier(1, &dst_barrier);
        }

        if (dump_this_frame && dump_src_readback != nullptr) {
            D3D12_RESOURCE_BARRIER src_copy_barrier{};
            src_copy_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            src_copy_barrier.Transition.pResource = src;
            src_copy_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            src_copy_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            src_copy_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            cmd_list->ResourceBarrier(1, &src_copy_barrier);

            D3D12_TEXTURE_COPY_LOCATION src_loc{};
            src_loc.pResource = src;
            src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_loc.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION dst_loc{};
            dst_loc.pResource = dump_src_readback.Get();
            dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst_loc.PlacedFootprint = dump_src_footprint;
            cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

            src_copy_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            src_copy_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cmd_list->ResourceBarrier(1, &src_copy_barrier);
        }

        if (transition_src) {
            src_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            src_barrier.Transition.StateAfter = src_state;
            cmd_list->ResourceBarrier(1, &src_barrier);
        }

        HRESULT hr = cmd_list->Close();
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDBLIT] command list close failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        ID3D12CommandList* lists[] = {cmd_list.Get()};
        queue->ExecuteCommandLists(1, lists);
        queue->Signal(fence.Get(), ++fence_value);
        waiting = true;

        if (dump_this_frame && dump_readback != nullptr) {
            wait(INFINITE);
            void* mapped = nullptr;
            D3D12_RANGE range{0, (SIZE_T)dump_total_bytes};
            HRESULT map_hr = dump_readback->Map(0, &range, &mapped);
            if (SUCCEEDED(map_hr) && mapped != nullptr) {
                CreateDirectoryA("E:\\tmp", nullptr);
                char dump_path[MAX_PATH]{};
                std::snprintf(dump_path, sizeof(dump_path), "E:\\tmp\\fh5_hudquad_dst_%03u.bmp", dump_seq);
                DumpStats stats{};
                const bool saved = save_texture_bmp(dump_path, static_cast<const uint8_t*>(mapped),
                    (uint32_t)dst_desc.Width, (uint32_t)dst_desc.Height,
                    dump_footprint.Footprint.RowPitch, dst_desc.Format, &stats);
                D3D12_RANGE empty{0, 0};
                dump_readback->Unmap(0, &empty);
                if (saved) {
                    spdlog::info("[VR-HUDBLIT] wrote HUD quad dst dump {} nonzero={} maxByte={} seq={}",
                        dump_path, stats.nonzero_bytes, (uint32_t)stats.max_byte, dump_seq);
                } else {
                    spdlog::warn("[VR-HUDBLIT] failed to write HUD quad dst dump seq={}", dump_seq);
                }
            } else {
                spdlog::warn("[VR-HUDBLIT] dst readback map failed: 0x{:08X}", (uint32_t)map_hr);
            }
        }

        if (dump_this_frame && dump_src_readback != nullptr) {
            wait(INFINITE);
            void* mapped = nullptr;
            D3D12_RANGE range{0, (SIZE_T)dump_src_total_bytes};
            HRESULT map_hr = dump_src_readback->Map(0, &range, &mapped);
            if (SUCCEEDED(map_hr) && mapped != nullptr) {
                CreateDirectoryA("E:\\tmp", nullptr);
                char dump_path[MAX_PATH]{};
                std::snprintf(dump_path, sizeof(dump_path), "E:\\tmp\\fh5_hudquad_src_%03u.bmp", dump_seq);
                DumpStats stats{};
                const bool saved = save_texture_bmp(dump_path, static_cast<const uint8_t*>(mapped),
                    (uint32_t)src_desc.Width, (uint32_t)src_desc.Height,
                    dump_src_footprint.Footprint.RowPitch, src_desc.Format, &stats);
                D3D12_RANGE empty{0, 0};
                dump_src_readback->Unmap(0, &empty);
                if (saved) {
                    spdlog::info("[VR-HUDBLIT] wrote HUD quad src dump {} nonzero={} maxByte={} seq={} fmt={}",
                        dump_path, stats.nonzero_bytes, (uint32_t)stats.max_byte, dump_seq, (int)src_desc.Format);
                } else {
                    spdlog::warn("[VR-HUDBLIT] failed to write HUD quad src dump seq={} fmt={}", dump_seq, (int)src_desc.Format);
                }
            } else {
                spdlog::warn("[VR-HUDBLIT] src readback map failed: 0x{:08X}", (uint32_t)map_hr);
            }
        }

        static std::atomic<uint64_t> s_last_log_ms{0};
        const uint64_t now_ms = ::GetTickCount64();
        uint64_t last_ms = s_last_log_ms.load(std::memory_order_relaxed);
        if (now_ms - last_ms >= 2000 &&
            s_last_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
            spdlog::info("[VR-HUDBLIT] blit src={}x{} fmt={} state=0x{:X} -> dst={}x{} fmt={}",
                (uint64_t)src_desc.Width, (uint32_t)src_desc.Height, (int)src_desc.Format, (uint32_t)src_state,
                (uint64_t)dst_desc.Width, (uint32_t)dst_desc.Height, (int)dst_desc.Format);
        }
        return true;
    }

    bool blit_diff(ID3D12Resource* final_src, ID3D12Resource* base_src, ID3D12Resource* dst,
                   ID3D12Device* device, ID3D12CommandQueue* queue,
                   D3D12_RESOURCE_STATES final_state, D3D12_RESOURCE_STATES base_state) {
        if (final_src == nullptr || base_src == nullptr || dst == nullptr || device == nullptr || queue == nullptr) {
            return false;
        }

        const D3D12_RESOURCE_DESC final_desc = final_src->GetDesc();
        const D3D12_RESOURCE_DESC base_desc = base_src->GetDesc();
        const D3D12_RESOURCE_DESC dst_desc = dst->GetDesc();
        if (final_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            base_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            dst_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            final_desc.SampleDesc.Count != 1 || base_desc.SampleDesc.Count != 1 || dst_desc.SampleDesc.Count != 1) {
            spdlog::warn("[VR-HUDDIFF] unsupported desc final_dim={} base_dim={} dst_dim={} samples={}/{}/{}",
                (int)final_desc.Dimension, (int)base_desc.Dimension, (int)dst_desc.Dimension,
                (uint32_t)final_desc.SampleDesc.Count, (uint32_t)base_desc.SampleDesc.Count,
                (uint32_t)dst_desc.SampleDesc.Count);
            return false;
        }

        const DXGI_FORMAT final_srv_format = SrvReadableFormat(final_desc.Format);
        const DXGI_FORMAT base_srv_format = SrvReadableFormat(base_desc.Format);
        if (final_srv_format == DXGI_FORMAT_UNKNOWN || base_srv_format == DXGI_FORMAT_UNKNOWN) {
            spdlog::warn("[VR-HUDDIFF] unsupported source formats final={} base={}",
                         (int)final_desc.Format, (int)base_desc.Format);
            return false;
        }

        if (!setup(device, dst_desc.Format) || pso_diff == nullptr) {
            return false;
        }

        wait(INFINITE);
        allocator->Reset();
        cmd_list->Reset(allocator.Get(), pso_diff.Get());

        static std::atomic<bool> s_diff_dump_written{false};
        bool dump_this_frame = false;
        bool dump_expected = false;
        if ((dst_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || dst_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
             dst_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || dst_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) &&
            s_diff_dump_written.compare_exchange_strong(dump_expected, true, std::memory_order_relaxed)) {
            dump_this_frame = true;
        }

        ComPtr<ID3D12Resource> dump_readback{};
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT dump_footprint{};
        UINT dump_rows = 0;
        UINT64 dump_row_bytes = 0;
        UINT64 dump_total_bytes = 0;
        if (dump_this_frame) {
            device->GetCopyableFootprints(&dst_desc, 0, 1, 0, &dump_footprint, &dump_rows, &dump_row_bytes, &dump_total_bytes);

            D3D12_HEAP_PROPERTIES hp{};
            hp.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC rb_desc{};
            rb_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rb_desc.Width = dump_total_bytes;
            rb_desc.Height = 1;
            rb_desc.DepthOrArraySize = 1;
            rb_desc.MipLevels = 1;
            rb_desc.SampleDesc.Count = 1;
            rb_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            HRESULT rb_hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rb_desc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dump_readback));
            if (FAILED(rb_hr) || dump_readback == nullptr) {
                spdlog::warn("[VR-HUDDIFF] readback buffer create failed: 0x{:08X}", (uint32_t)rb_hr);
                dump_this_frame = false;
            }
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC final_srv_desc{};
        final_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        final_srv_desc.Format = final_srv_format;
        final_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        final_srv_desc.Texture2D.MipLevels = final_desc.MipLevels;
        final_srv_desc.Texture2D.MostDetailedMip = 0;
        final_srv_desc.Texture2D.PlaneSlice = 0;
        final_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

        D3D12_SHADER_RESOURCE_VIEW_DESC base_srv_desc{};
        base_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        base_srv_desc.Format = base_srv_format;
        base_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        base_srv_desc.Texture2D.MipLevels = base_desc.MipLevels;
        base_srv_desc.Texture2D.MostDetailedMip = 0;
        base_srv_desc.Texture2D.PlaneSlice = 0;
        base_srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

        D3D12_CPU_DESCRIPTOR_HANDLE srv0 = srv_heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE srv1 = srv0;
        srv1.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device->CreateShaderResourceView(final_src, &final_srv_desc, srv0);
        device->CreateShaderResourceView(base_src, &base_srv_desc, srv1);
        device->CreateRenderTargetView(dst, nullptr, rtv_heap->GetCPUDescriptorHandleForHeapStart());

        D3D12_RESOURCE_BARRIER barriers[2]{};
        UINT nb = 0;
        const bool transition_final =
            (final_state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        const bool transition_base =
            base_src != final_src &&
            (base_state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (transition_final) {
            barriers[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[nb].Transition.pResource = final_src;
            barriers[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[nb].Transition.StateBefore = final_state;
            barriers[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            ++nb;
        }
        if (transition_base) {
            barriers[nb].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[nb].Transition.pResource = base_src;
            barriers[nb].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[nb].Transition.StateBefore = base_state;
            barriers[nb].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            ++nb;
        }
        if (nb) {
            cmd_list->ResourceBarrier(nb, barriers);
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        cmd_list->ClearRenderTargetView(rtv, clear, 0, nullptr);
        cmd_list->SetGraphicsRootSignature(root_sig.Get());
        ID3D12DescriptorHeap* heaps[] = {srv_heap.Get()};
        cmd_list->SetDescriptorHeaps(1, heaps);
        cmd_list->SetGraphicsRootDescriptorTable(0, srv_heap->GetGPUDescriptorHandleForHeapStart());
        cmd_list->SetGraphicsRoot32BitConstant(1, fh5cb::ctl_hud_flipv() ? 1u : 0u, 0);

        D3D12_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)dst_desc.Width;
        viewport.Height = (float)dst_desc.Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        D3D12_RECT scissor{0, 0, (LONG)dst_desc.Width, (LONG)dst_desc.Height};
        cmd_list->RSSetViewports(1, &viewport);
        cmd_list->RSSetScissorRects(1, &scissor);
        cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd_list->DrawInstanced(3, 1, 0, 0);

        if (dump_this_frame && dump_readback != nullptr) {
            D3D12_RESOURCE_BARRIER dst_barrier{};
            dst_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            dst_barrier.Transition.pResource = dst;
            dst_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            dst_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            dst_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            cmd_list->ResourceBarrier(1, &dst_barrier);

            D3D12_TEXTURE_COPY_LOCATION src_loc{};
            src_loc.pResource = dst;
            src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_loc.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION dst_loc{};
            dst_loc.pResource = dump_readback.Get();
            dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst_loc.PlacedFootprint = dump_footprint;
            cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

            dst_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            dst_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmd_list->ResourceBarrier(1, &dst_barrier);
        }

        D3D12_RESOURCE_BARRIER restore[2]{};
        UINT nr = 0;
        if (transition_final) {
            restore[nr].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            restore[nr].Transition.pResource = final_src;
            restore[nr].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            restore[nr].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            restore[nr].Transition.StateAfter = final_state;
            ++nr;
        }
        if (transition_base) {
            restore[nr].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            restore[nr].Transition.pResource = base_src;
            restore[nr].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            restore[nr].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            restore[nr].Transition.StateAfter = base_state;
            ++nr;
        }
        if (nr) {
            cmd_list->ResourceBarrier(nr, restore);
        }

        HRESULT hr = cmd_list->Close();
        if (FAILED(hr)) {
            spdlog::error("[VR-HUDDIFF] command list close failed: 0x{:08X}", (uint32_t)hr);
            return false;
        }

        ID3D12CommandList* lists[] = {cmd_list.Get()};
        queue->ExecuteCommandLists(1, lists);
        queue->Signal(fence.Get(), ++fence_value);
        waiting = true;

        if (dump_this_frame && dump_readback != nullptr) {
            wait(INFINITE);
            void* mapped = nullptr;
            D3D12_RANGE range{0, (SIZE_T)dump_total_bytes};
            HRESULT map_hr = dump_readback->Map(0, &range, &mapped);
            if (SUCCEEDED(map_hr) && mapped != nullptr) {
                CreateDirectoryA("E:\\tmp", nullptr);
                const char* dump_path = "E:\\tmp\\fh5_hudquad_diff_dump.bmp";
                const bool saved = save_texture_bmp(dump_path, static_cast<const uint8_t*>(mapped),
                    (uint32_t)dst_desc.Width, (uint32_t)dst_desc.Height,
                    dump_footprint.Footprint.RowPitch, dst_desc.Format);
                D3D12_RANGE empty{0, 0};
                dump_readback->Unmap(0, &empty);
                if (saved) {
                    spdlog::info("[VR-HUDDIFF] wrote one-shot HUD quad diff dump {}", dump_path);
                } else {
                    spdlog::warn("[VR-HUDDIFF] failed to write one-shot HUD quad diff dump");
                }
            } else {
                spdlog::warn("[VR-HUDDIFF] readback map failed: 0x{:08X}", (uint32_t)map_hr);
            }
        }

        static std::atomic<uint64_t> s_last_log_ms{0};
        const uint64_t now_ms = ::GetTickCount64();
        uint64_t last_ms = s_last_log_ms.load(std::memory_order_relaxed);
        if (now_ms - last_ms >= 2000 &&
            s_last_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
            spdlog::info("[VR-HUDDIFF] blit final={}x{} fmt={} state=0x{:X} base={}x{} fmt={} state=0x{:X} -> dst={}x{} fmt={}",
                (uint64_t)final_desc.Width, (uint32_t)final_desc.Height, (int)final_desc.Format, (uint32_t)final_state,
                (uint64_t)base_desc.Width, (uint32_t)base_desc.Height, (int)base_desc.Format, (uint32_t)base_state,
                (uint64_t)dst_desc.Width, (uint32_t)dst_desc.Height, (int)dst_desc.Format);
        }
        return true;
    }

    ComPtr<ID3D12Device> cached_device{};
    DXGI_FORMAT cached_rt_format{DXGI_FORMAT_UNKNOWN};
    ComPtr<ID3D12CommandAllocator> allocator{};
    ComPtr<ID3D12GraphicsCommandList> cmd_list{};
    ComPtr<ID3D12Fence> fence{};
    ComPtr<ID3D12DescriptorHeap> srv_heap{};
    ComPtr<ID3D12DescriptorHeap> rtv_heap{};
    ComPtr<ID3D12RootSignature> root_sig{};
    ComPtr<ID3D12PipelineState> pso{};
    ComPtr<ID3D12PipelineState> pso_diff{};
    HANDLE fence_event{nullptr};
    UINT64 fence_value{0};
    bool waiting{false};
};

D3D12Component::OpenXR::OpenXR() = default;
D3D12Component::OpenXR::~OpenXR() = default;

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

    // Install the UI-draw redirect once device+swapchain are live (idempotent; gated by uiredirect at runtime).
    fh5cb::ui_redirect_install(device, swapchain);

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

    // Register the eye-source resource with the UI-redirect: this exact resource is what the game renders the
    // world composite + HUD into (PIX-proven, FH5InGame.wpix), so the redirect anchors on it by resource to
    // peel the HUD onto the quad. Accumulates the swapchain's buffer set across frames (idempotent).
    fh5cb::register_eye_source(eye_texture);

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

    const int ui_redirect_mode = fh5cb::ctl_ui_redirect_mode();
    const bool phase_locked_hud = ui_redirect_mode == 20 || ui_redirect_mode == 21 ||
                                  ui_redirect_mode == 22 || ui_redirect_mode == 23 ||
                                  ui_redirect_mode == 24 || ui_redirect_mode == 25 ||
                                  ui_redirect_mode == 26 || ui_redirect_mode == 27 ||
                                  ui_redirect_mode == 29;
    // Modes 23/24/25/27 only need one stable AER phase. Refresh on the left pass, then submit that image to both
    // eyes; mode 26 honors the live phase control because its PSO mirror may be populated by either AER phase.
    const int hud_phase = (ui_redirect_mode == 23 || ui_redirect_mode == 24 ||
                           ui_redirect_mode == 25 || ui_redirect_mode == 27)
        ? 0
        : fh5cb::ctl_hud_phase();
    if (!fh5cb::ctl_hud_quad()) {
        m_openxr.hud_anchor_ready = false;
    }
    ID3D12Resource* projection_src = eye_texture;
    D3D12_RESOURCE_STATES projection_src_state = D3D12_RESOURCE_STATE_PRESENT;
    // Crysis/Far Cry/FO2 VR pattern: submit a clean world eye, then put the UI capture on a quad.
    // Mode 25 keeps its proven late full-HUD mirror; mode 27 steals OverlayRenderer12 draws into the quad RT.
    if (ui_redirect_mode == 18 || ui_redirect_mode == 19 || ui_redirect_mode == 24 ||
        ui_redirect_mode == 25 || ui_redirect_mode == 27) {
        uint32_t pw = 0, ph = 0, pf = 0;
        if (ID3D12Resource* pre_ui = fh5cb::pre_ui_eye_candidate_for(eye_texture, &pw, &ph, &pf)) {
            projection_src = pre_ui;
            projection_src_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            static std::atomic<uint64_t> s_preui_projection_log_ms{0};
            const uint64_t now_ms = ::GetTickCount64();
            uint64_t last_ms = s_preui_projection_log_ms.load(std::memory_order_relaxed);
            if (now_ms - last_ms >= 2000 &&
                s_preui_projection_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                spdlog::info("[VR-COPY] using pre-UI eye projection source eye={} src=0x{:X}[{}x{} f{}]",
                    applied_eye, reinterpret_cast<uintptr_t>(pre_ui), pw, ph, pf);
            }
        }
    }

    m_openxr.copy(vr, applied_eye, projection_src, device, command_queue, 0, projection_src_state);

    if (phase_locked_hud && fh5cb::ctl_hud_quad() && applied_eye == hud_phase && applied_eye != 1) {
        uint32_t mw = 0, mh = 0, mf = 0;
        D3D12_RESOURCE_STATES source_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        ID3D12Resource* source = nullptr;
        if (ui_redirect_mode == 24) {
            if (ID3D12Resource* pre_ui = fh5cb::pre_ui_eye_candidate_for(eye_texture, &mw, &mh, &mf)) {
                const bool copied = m_openxr.copy_hud_delta(vr, eye_texture, pre_ui, device, command_queue,
                                                            D3D12_RESOURCE_STATE_PRESENT,
                                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                if (copied) {
                    m_openxr.hud_layer_ready = true;
                    m_openxr.hud_layer_phase = applied_eye;
                    static std::atomic<uint64_t> s_phase_delta_refresh_log_ms{0};
                    const uint64_t now_ms = ::GetTickCount64();
                    uint64_t last_ms = s_phase_delta_refresh_log_ms.load(std::memory_order_relaxed);
                    if (now_ms - last_ms >= 2000 &&
                        s_phase_delta_refresh_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                        spdlog::info("[VR-HUDQUAD] phase-locked delta refresh eye={} preUI=0x{:X}[{}x{} f{}]",
                            applied_eye, reinterpret_cast<uintptr_t>(pre_ui), mw, mh, mf);
                    }
                }
            } else {
                static std::atomic<uint64_t> s_phase_delta_wait_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_phase_delta_wait_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_phase_delta_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] waiting for phase-locked pre-UI delta source eye={}", applied_eye);
                }
            }
            return false;
        }
        if (ui_redirect_mode == 29) {
            source = fh5cb::overlay_native_target_candidate(&mw, &mh, &mf);
            source_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (ui_redirect_mode == 27) {
            source = fh5cb::overlay_composite_candidate(&mw, &mh, &mf);
            source_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (ui_redirect_mode == 23 || ui_redirect_mode == 26) {
            source = fh5cb::ui_mirror_candidate(&mw, &mh, &mf);
            source_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (ui_redirect_mode == 22) {
            source = fh5cb::ui_atlas_candidate(&mw, &mh, &mf);
            source_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        } else if (ui_redirect_mode == 21) {
            source = fh5cb::ui_final_sample_candidate(&mw, &mh, &mf);
            source_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        } else {
            source = fh5cb::ui_final_mirror_candidate(&mw, &mh, &mf);
        }
        if (source != nullptr) {
            const bool copied = m_openxr.copy_hud(vr, source, device, command_queue,
                                                  source_state,
                                                  D3D12_CPU_DESCRIPTOR_HANDLE{});
            if (copied) {
                m_openxr.hud_layer_ready = true;
                m_openxr.hud_layer_phase = applied_eye;
                static std::atomic<uint64_t> s_phase_refresh_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_phase_refresh_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_phase_refresh_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] phase-locked refresh eye={} source=0x{:X}[{}x{} f{}]",
                        applied_eye, reinterpret_cast<uintptr_t>(source), mw, mh, mf);
                }
            }
        } else {
            static std::atomic<uint64_t> s_phase_wait_log_ms{0};
            const uint64_t now_ms = ::GetTickCount64();
            uint64_t last_ms = s_phase_wait_log_ms.load(std::memory_order_relaxed);
            if (now_ms - last_ms >= 2000 &&
                s_phase_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                const char* phase_source = (ui_redirect_mode == 27) ? "overlay-steal" :
                    (ui_redirect_mode == 29) ? "overlay-native-target" :
                    (ui_redirect_mode == 23 || ui_redirect_mode == 26) ? "pso-mirror" :
                    (ui_redirect_mode == 22) ? "atlas" :
                    (ui_redirect_mode == 21 ? "sample" : "mirror");
                spdlog::info("[VR-HUDQUAD] waiting for phase-locked final UI {} source eye={}",
                    phase_source, applied_eye);
            }
        }
    }

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
        // submit it as a fixed LOCAL-space quad alongside the eye projection. INITIAL source = the
        // engine backbuffer (proves swapchain/copy/quad/compositing end-to-end with a recognizable image);
        // this is replaced by the UI-only texture once FH5's UI render target is identified. hud_quad must
        // outlive the end_frame call below, so it is declared here in the submit scope.
        std::vector<XrCompositionLayerBaseHeader*> extra_layers{};
        XrCompositionLayerQuad hud_quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        // Quad source: the UI-only redirect RT (RENDER_TARGET) when ui-redirect is active and has captured UI
        // this frame; otherwise the engine backbuffer (PRESENT) as the validation placeholder.
        ID3D12Resource* hud_src = eye_texture;
        D3D12_RESOURCE_STATES hud_src_state = D3D12_RESOURCE_STATE_PRESENT;
        D3D12_CPU_DESCRIPTOR_HANDLE hud_src_srv{};
        // PREFERRED: the redirected UI RT (HUD-only) when the signature-anchored draw-redirect is capturing the
        // HUD. hudplane=-2 is an explicit diagnostic selector for the UI-lineage composite SRV candidate.
        ID3D12Resource* hud_delta_base = nullptr;
        bool hud_delta = false;
        bool hud_source_ready = true;
        const int recenter_seq = fh5cb::ctl_recenter_seq();
        if (m_openxr.hud_anchor_ready &&
            m_openxr.hud_anchor_recenter_seq >= 0 &&
            recenter_seq != m_openxr.hud_anchor_recenter_seq) {
            m_openxr.hud_anchor_ready = false;
            m_openxr.hud_anchor_recenter_seq = recenter_seq;
            spdlog::info("[VR-HUDQUAD] recenter seq changed; rebuilding fixed LOCAL anchor seq={}", recenter_seq);
        }
        if (ui_redirect_mode == 11) {
            uint32_t mw = 0, mh = 0, mf = 0;
            D3D12_CPU_DESCRIPTOR_HANDLE srv{};
            if (ID3D12Resource* overlay = fh5cb::overlay_srv_candidate(&srv, &mw, &mh, &mf)) {
                hud_src = overlay;
                hud_src_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                hud_src_srv = srv;
                static std::atomic<uint64_t> s_overlay_src_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_overlay_src_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_overlay_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] using OverlayRenderer12 SRV source 0x{:X}[{}x{} f{}] srv=0x{:X}",
                        reinterpret_cast<uintptr_t>(overlay), mw, mh, mf, (uintptr_t)srv.ptr);
                }
            }
        } else if (ui_redirect_mode == 19 || ui_redirect_mode == 27 || ui_redirect_mode == 29) {
            uint32_t mw = 0, mh = 0, mf = 0;
            ID3D12Resource* overlay_rt = (ui_redirect_mode == 29)
                ? fh5cb::overlay_native_target_candidate(&mw, &mh, &mf)
                : fh5cb::overlay_composite_candidate(&mw, &mh, &mf);
            if (overlay_rt) {
                hud_src = overlay_rt;
                hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                static std::atomic<uint64_t> s_overlay_rt_src_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_overlay_rt_src_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_overlay_rt_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] using OverlayRenderer12 {} RT source 0x{:X}[{}x{} f{}] mode={}",
                        ui_redirect_mode == 29 ? "native-target" : "composed",
                        reinterpret_cast<uintptr_t>(overlay_rt), mw, mh, mf, ui_redirect_mode);
                }
            } else {
                hud_source_ready = false;
                static std::atomic<uint64_t> s_overlay_rt_wait_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_overlay_rt_wait_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_overlay_rt_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] waiting for OverlayRenderer12 composed RT source (mode {})", ui_redirect_mode);
                }
            }
        } else if (ui_redirect_mode == 10 || ui_redirect_mode == 18 || ui_redirect_mode == 24) {
            uint32_t mw = 0, mh = 0, mf = 0;
            if (ID3D12Resource* pre_ui = fh5cb::pre_ui_eye_candidate_for(eye_texture, &mw, &mh, &mf)) {
                hud_src = eye_texture;
                hud_src_state = D3D12_RESOURCE_STATE_PRESENT;
                hud_delta_base = pre_ui;
                hud_delta = true;
                static std::atomic<uint64_t> s_preui_src_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_preui_src_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_preui_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] using pre-UI delta source final=0x{:X} base=0x{:X}[{}x{} f{}]",
                        reinterpret_cast<uintptr_t>(eye_texture), reinterpret_cast<uintptr_t>(pre_ui), mw, mh, mf);
                }
            } else {
                if (phase_locked_hud && applied_eye != hud_phase && m_openxr.hud_layer_ready) {
                    hud_source_ready = true;
                } else {
                    hud_source_ready = false;
                }
                static std::atomic<uint64_t> s_preui_wait_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_preui_wait_log_ms.load(std::memory_order_relaxed);
                if (!hud_source_ready && now_ms - last_ms >= 2000 &&
                    s_preui_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] waiting for pre-UI delta source (mode {})", ui_redirect_mode);
                }
            }
        } else if (ui_redirect_mode == 9) {
            uint32_t mw = 0, mh = 0, mf = 0;
            if (ID3D12Resource* atlas = fh5cb::ui_atlas_candidate(&mw, &mh, &mf)) {
                hud_src = atlas;
                hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                static std::atomic<uint64_t> s_atlas_src_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_atlas_src_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_atlas_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] using direct UI atlas RT source 0x{:X}[{}x{} f{}]",
                        reinterpret_cast<uintptr_t>(atlas), mw, mh, mf);
                }
            }
        } else if (ui_redirect_mode == 16 || ui_redirect_mode == 17 || ui_redirect_mode == 20 ||
                   ui_redirect_mode == 21 || ui_redirect_mode == 22 || ui_redirect_mode == 23 ||
                   ui_redirect_mode == 25 || ui_redirect_mode == 26 || ui_redirect_mode == 28) {
            uint32_t mw = 0, mh = 0, mf = 0;
            ID3D12Resource* source = nullptr;
            if (ui_redirect_mode == 17 || ui_redirect_mode == 20 || ui_redirect_mode == 21 ||
                ui_redirect_mode == 22 || ui_redirect_mode == 23 || ui_redirect_mode == 25 ||
                ui_redirect_mode == 26 || ui_redirect_mode == 28) {
                if (ui_redirect_mode == 23 || ui_redirect_mode == 26) {
                    source = fh5cb::ui_mirror_candidate(&mw, &mh, &mf);
                } else if (ui_redirect_mode == 22) {
                    source = fh5cb::ui_atlas_candidate(&mw, &mh, &mf);
                } else if (ui_redirect_mode == 21) {
                    source = fh5cb::ui_final_sample_candidate(&mw, &mh, &mf);
                } else {
                    source = fh5cb::ui_final_mirror_candidate(&mw, &mh, &mf);
                }
                if (source) {
                    hud_src = source;
                    hud_src_state = (ui_redirect_mode == 21)
                        ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                        : D3D12_RESOURCE_STATE_RENDER_TARGET;
                    static std::atomic<uint64_t> s_final_mirror_src_log_ms{0};
                    const uint64_t now_ms = ::GetTickCount64();
                    uint64_t last_ms = s_final_mirror_src_log_ms.load(std::memory_order_relaxed);
                    if (now_ms - last_ms >= 2000 &&
                        s_final_mirror_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                        const char* source_name = (ui_redirect_mode == 28) ? "final-steal" :
                            (ui_redirect_mode == 23 || ui_redirect_mode == 26) ? "pso-mirror" :
                            (ui_redirect_mode == 22) ? "atlas" :
                            (ui_redirect_mode == 21 ? "sample" : "mirror");
                        spdlog::info("[VR-HUDQUAD] using final UI {} source 0x{:X}[{}x{} f{}]",
                            source_name,
                            reinterpret_cast<uintptr_t>(source), mw, mh, mf);
                    }
                }
            }
            if (!source && ui_redirect_mode != 28) {
                if (ID3D12Resource* lineage = fh5cb::ui_lineage_candidate()) {
                    hud_src = lineage;
                    hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    static std::atomic<uint64_t> s_lineage_src_log_ms{0};
                    const uint64_t now_ms = ::GetTickCount64();
                    uint64_t last_ms = s_lineage_src_log_ms.load(std::memory_order_relaxed);
                    if (now_ms - last_ms >= 2000 &&
                        s_lineage_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                        spdlog::info("[VR-HUDQUAD] using broad UI lineage source 0x{:X}",
                            reinterpret_cast<uintptr_t>(lineage));
                    }
                } else if (ID3D12Resource* mirror = fh5cb::ui_mirror_candidate(&mw, &mh, &mf)) {
                    hud_src = mirror;
                    hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    static std::atomic<uint64_t> s_broad_mirror_src_log_ms{0};
                    const uint64_t now_ms = ::GetTickCount64();
                    uint64_t last_ms = s_broad_mirror_src_log_ms.load(std::memory_order_relaxed);
                    if (now_ms - last_ms >= 2000 &&
                        s_broad_mirror_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                        spdlog::info("[VR-HUDQUAD] using fallback mirrored UI source 0x{:X}[{}x{} f{}]",
                            reinterpret_cast<uintptr_t>(mirror), mw, mh, mf);
                    }
                }
            } else if (!source) {
                hud_source_ready = false;
                static std::atomic<uint64_t> s_final_steal_wait_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_final_steal_wait_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_final_steal_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] waiting for full-size final UI steal source");
                }
            }
        } else if (ui_redirect_mode == 7 || ui_redirect_mode == 8 ||
                   ui_redirect_mode == 12 || ui_redirect_mode == 13 ||
                   ui_redirect_mode == 14 || ui_redirect_mode == 15) {
            uint32_t mw = 0, mh = 0, mf = 0;
            if (ID3D12Resource* mirror = fh5cb::ui_mirror_candidate(&mw, &mh, &mf)) {
                hud_src = mirror;
                hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                static std::atomic<uint64_t> s_mirror_src_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_mirror_src_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_mirror_src_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] using mirrored UI source 0x{:X}[{}x{} f{}]",
                        reinterpret_cast<uintptr_t>(mirror), mw, mh, mf);
                }
            }
        } else if (ui_redirect_mode == 30) {
            // Engine-seam (vf54 bracket). Gameplay HUD is stolen into a FORMAT-MATCHED mirror (prefer it when
            // fresh); menus/showroom UI lands in the single format-matched UI RT. Either way it is HUD-only.
            uint32_t mw = 0, mh = 0, mf = 0;
            if (ID3D12Resource* mirror = fh5cb::ui_final_mirror_candidate(&mw, &mh, &mf)) {
                hud_src = mirror;
                hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                static std::atomic<uint64_t> s_seam_mir_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_seam_mir_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_seam_mir_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] seam(30) mirror UI source 0x{:X}[{}x{} f{}]",
                        reinterpret_cast<uintptr_t>(mirror), mw, mh, mf);
                }
            } else if (ID3D12Resource* uirt = fh5cb::ui_redirect_target()) {
                hud_src = uirt;
                hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
                static std::atomic<uint64_t> s_seam_rt_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_seam_rt_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_seam_rt_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] seam(30) single UI RT source 0x{:X}",
                        reinterpret_cast<uintptr_t>(uirt));
                }
            } else {
                hud_source_ready = false;
            }
        } else if (fh5cb::ui_redirect_active()) {
            if (ID3D12Resource* uirt = fh5cb::ui_redirect_target()) {
                hud_src = uirt; hud_src_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
            }
        } else if (fh5cb::ctl_hud_plane() == -2) {
            if (ID3D12Resource* lineage = fh5cb::ui_lineage_candidate()) {
                hud_src = lineage; hud_src_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            }
        }
        bool hud_copied = false;
        if (fh5cb::ctl_hud_quad() && hud_source_ready) {
            const bool refresh_this_phase = !phase_locked_hud || applied_eye == hud_phase;
            if (refresh_this_phase) {
                hud_copied = hud_delta
                    ? m_openxr.copy_hud_delta(vr, hud_src, hud_delta_base, device, command_queue,
                                              hud_src_state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
                    : m_openxr.copy_hud(vr, hud_src, device, command_queue, hud_src_state, hud_src_srv);
                if (hud_copied) {
                    m_openxr.hud_layer_ready = true;
                    m_openxr.hud_layer_phase = applied_eye;
                }
            } else if (phase_locked_hud && m_openxr.hud_layer_ready) {
                hud_copied = true;
                static std::atomic<uint64_t> s_phase_reuse_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_phase_reuse_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_phase_reuse_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] reusing phase-locked HUD image phase={} submitEye={}",
                        m_openxr.hud_layer_phase, applied_eye);
                }
            }
        }
        if (hud_copied) {
            const float aspect = (m_openxr.hud_height > 0)
                ? (float)m_openxr.hud_width / (float)m_openxr.hud_height : (16.0f / 9.0f);
            const float quad_w = fh5cb::ctl_hud_w();        // metres wide (live-tunable)
            const float quad_h = quad_w / aspect;           // aspect-correct height
            const XrVector3f hud_offset{fh5cb::ctl_hud_x(), fh5cb::ctl_hud_y(), fh5cb::ctl_hud_z()};
            auto& openxr = vr->m_openxr;
            if (!m_openxr.hud_anchor_ready && openxr != nullptr &&
                ReadyToLockHudAnchor() && ValidSpacePose(openxr->view_space_location)) {
                m_openxr.hud_anchor_origin = openxr->view_space_location.pose.position;
                m_openxr.hud_anchor_orientation = BuildYawOnlyAnchor(openxr->view_space_location.pose.orientation);
                m_openxr.hud_anchor_ready = true;
                m_openxr.hud_anchor_recenter_seq = recenter_seq;
                const XrVector3f anchor_forward = RotateVector(m_openxr.hud_anchor_orientation, {0.0f, 0.0f, -1.0f});
                spdlog::info("[VR-HUDQUAD] fixed LOCAL anchor origin=({:.2f},{:.2f},{:.2f}) forward=({:.2f},{:.2f},{:.2f})",
                    m_openxr.hud_anchor_origin.x, m_openxr.hud_anchor_origin.y, m_openxr.hud_anchor_origin.z,
                    anchor_forward.x, anchor_forward.y, anchor_forward.z);
            }
            if (!m_openxr.hud_anchor_ready) {
                static std::atomic<uint64_t> s_hud_anchor_wait_log_ms{0};
                const uint64_t now_ms = ::GetTickCount64();
                uint64_t last_ms = s_hud_anchor_wait_log_ms.load(std::memory_order_relaxed);
                if (now_ms - last_ms >= 2000 &&
                    s_hud_anchor_wait_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                    spdlog::info("[VR-HUDQUAD] waiting for active FH5 camera and valid LOCAL-space pose before submitting fixed quad camHits={}",
                        fh5cb::cam_hits());
                }
            } else {
                const XrPosef hud_pose = HudPoseFromAnchor(m_openxr.hud_anchor_origin,
                    m_openxr.hud_anchor_orientation, hud_offset);
            // Opaque during backbuffer validation (the backbuffer alpha may be 0 -> a source-alpha quad would
            // be fully transparent/invisible). Switch to source-alpha once the quad source is a real UI texture.
            // Real UI source: source-alpha blend. FH5 UI draws onto a cleared-transparent RT, so the captured
            // content is PREMULTIPLIED (hudpremul=on, default) -> source-alpha WITHOUT the unpremultiplied bit.
            // hudpremul=off restores straight-alpha handling. hudopaque=on stays fully opaque for validation.
            hud_quad.layerFlags = fh5cb::ctl_hud_opaque()
                ? 0
                : (XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                   (fh5cb::ctl_hud_premul() ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT));
            hud_quad.space = vr->m_openxr->stage_space;      // fixed tracking/LOCAL-space panel
            hud_quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            hud_quad.subImage.swapchain = m_openxr.hud_handle;
            hud_quad.subImage.imageArrayIndex = 0;
            hud_quad.subImage.imageRect.offset = {0, 0};
            hud_quad.subImage.imageRect.extent = {m_openxr.hud_width, m_openxr.hud_height};
            hud_quad.pose = hud_pose;
            hud_quad.size = {quad_w, quad_h};
            extra_layers.push_back((XrCompositionLayerBaseHeader*)&hud_quad);
            static std::atomic<uint64_t> s_hud_log_ms{0};
            const uint64_t now_ms = ::GetTickCount64();
            uint64_t last_ms = s_hud_log_ms.load(std::memory_order_relaxed);
            if (now_ms - last_ms >= 2000 && s_hud_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
                spdlog::info("[VR-HUDQUAD] fixed quad {:.2f}x{:.2f}m anchorOffset=({:.2f},{:.2f},{:.2f}) worldPos=({:.2f},{:.2f},{:.2f}) opaque={} (swapchain {}x{})",
                    quad_w, quad_h, hud_offset.x, hud_offset.y, hud_offset.z,
                    hud_pose.position.x, hud_pose.position.y, hud_pose.position.z,
                    fh5cb::ctl_hud_opaque() ? 1 : 0, m_openxr.hud_width, m_openxr.hud_height);
            }
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
    // Reset the UI-redirect per-frame backbuffer-bind counter once per present (frame boundary).
    fh5cb::ui_redirect_on_present();
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

    // UI/HUD quad swapchain (separate from the eye set), sized to the backbuffer for a 1:1 CopyResource.
    // CRITICAL: it MUST match the HUD-source format (the eye backbuffer / UI RT, both = the live backbuffer
    // format). FH5 free-roam gameplay is HDR R10G10B10A2 (fmt 24); a hardcoded R8G8B8A8_SRGB HUD swapchain
    // makes CopyResource(R10A2 -> RGBA8_SRGB) INVALID (cross-family) -> the quad showed magenta garbage even
    // though the eye copy worked. Use the SAME backbuffer->xr format mapping the eye swapchains use (R8/B8 ->
    // _SRGB so the compositor sRGB-decodes; R10A2 stays R10A2 -- same family as the source -> copy valid).
    DXGI_FORMAT hud_fmt;
    switch (bb_fmt_probe) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  hud_fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  hud_fmt = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:    hud_fmt = DXGI_FORMAT_R10G10B10A2_UNORM;   break;  // HDR gameplay
        default:                               hud_fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
    }
    if (auto hud_err = create_hud_swapchain(vr, device, width, height, hud_fmt)) {
        spdlog::warn("[VR] HUD quad swapchain not created: {} (eye rendering unaffected)", *hud_err);
    } else {
        spdlog::info("[VR] HUD quad swapchain fmt={} (matched to backbuffer fmt={})", (int)hud_fmt, (int)bb_fmt_probe);
    }

    return std::nullopt;
}

std::optional<std::string> D3D12Component::OpenXR::create_hud_swapchain(VR* vr, ID3D12Device* device,
                                                                       uint32_t width, uint32_t height,
                                                                       DXGI_FORMAT xr_fmt) {
    std::scoped_lock _{this->mtx};
    this->hud_ready = false;
    this->hud_layer_ready = false;
    this->hud_layer_phase = -1;
    this->hud_anchor_ready = false;
    this->hud_anchor_recenter_seq = -1;

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
    this->hud_layer_ready = false;
    this->hud_layer_phase = -1;
    this->hud_anchor_ready = false;
    this->hud_anchor_recenter_seq = -1;
    spdlog::info("[VR] HUD quad swapchain created ({}x{}, {} images, fmt {})", width, height, image_count, (int)xr_fmt);
    return std::nullopt;
}

void D3D12Component::OpenXR::destroy_swapchains(VR* vr) {
    std::scoped_lock _{this->mtx};

    if (this->contexts.empty()) {
        return;
    }

    spdlog::info("[VR] Destroying OpenXR swapchains");

    // Drain any in-flight GPU copies/blits BEFORE freeing the copiers, blitter, RTs and swapchains. At a scene
    // transition (e.g. My-Cars hub -> free-roam) FH5 recreates its swapchain, which lands us here while the GPU
    // may still be mid-copy into an eye/HUD resource we're about to release -> use-after-free / DEVICE_REMOVED.
    // The fence waits are idempotent and cheap when nothing is in flight. (mtx is already held; inline the waits
    // rather than re-entering wait_for_all_copies to stay agnostic to the mutex's recursiveness.)
    for (auto& ctx : this->contexts) {
        for (auto& copier : ctx.copiers) { if (copier) copier->wait(INFINITE); }
    }
    for (auto& copier : this->hud_ctx.copiers) { if (copier) copier->wait(INFINITE); }
    if (this->hud_blitter != nullptr) { this->hud_blitter->wait(INFINITE); }

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
    this->hud_layer_ready = false;
    this->hud_layer_phase = -1;
    this->hud_anchor_ready = false;
    this->hud_anchor_recenter_seq = -1;
    this->hud_blitter.reset();
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
    for (auto& copier : this->hud_ctx.copiers) {
        if (copier != nullptr) {
            copier->wait(INFINITE);
        }
    }
    if (this->hud_blitter != nullptr) {
        this->hud_blitter->wait(INFINITE);
    }
}

void D3D12Component::OpenXR::copy(VR* vr, uint32_t swapchain_idx, ID3D12Resource* src,
                                  ID3D12Device* device, ID3D12CommandQueue* queue, int x_shift,
                                  D3D12_RESOURCE_STATES src_state) {
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

    // Copy src (engine backbuffer/final eye, or mode-18 pre-UI eye) -> runtime image (RENDER_TARGET).
    auto& copier = ctx.copiers[texture_index];
    copier->wait(INFINITE);
    copier->copy(src, ctx.textures[texture_index].texture,
                 src_state, D3D12_RESOURCE_STATE_RENDER_TARGET, x_shift);
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

bool D3D12Component::OpenXR::copy_hud(VR* vr, ID3D12Resource* src, ID3D12Device* device, ID3D12CommandQueue* queue,
                                     D3D12_RESOURCE_STATES src_state, D3D12_CPU_DESCRIPTOR_HANDLE src_srv) {
    std::scoped_lock _{this->mtx};

    auto& openxr = vr->m_openxr;
    if (!this->hud_ready || this->hud_handle == XR_NULL_HANDLE || openxr == nullptr || src == nullptr) {
        return false;
    }
    if (openxr->frame_state.shouldRender != XR_TRUE || !openxr->frame_began) {
        return false;
    }
    if (this->hud_ctx.textures.empty() || this->hud_ctx.textures[0].texture == nullptr) {
        return false;
    }

    const D3D12_RESOURCE_DESC src_desc = src->GetDesc();
    const D3D12_RESOURCE_DESC dst_desc = this->hud_ctx.textures[0].texture->GetDesc();
    const bool use_blit = src_srv.ptr != 0 || !CopyResourceCompatible(src_desc, dst_desc);
    if (use_blit) {
        static std::atomic<uint64_t> s_bad_hud_copy_log_ms{0};
        const uint64_t now_ms = ::GetTickCount64();
        uint64_t last_ms = s_bad_hud_copy_log_ms.load(std::memory_order_relaxed);
        if (now_ms - last_ms >= 2000 &&
            s_bad_hud_copy_log_ms.compare_exchange_strong(last_ms, now_ms, std::memory_order_relaxed)) {
            spdlog::info("[VR-HUDQUAD] using shader blit for incompatible CopyResource src={}x{}x{} mips={} fmt={} samples={} -> dst={}x{}x{} mips={} fmt={} samples={} state=0x{:X}",
                (uint64_t)src_desc.Width, (uint32_t)src_desc.Height, (uint32_t)src_desc.DepthOrArraySize,
                (uint32_t)src_desc.MipLevels, (int)src_desc.Format, (uint32_t)src_desc.SampleDesc.Count,
                (uint64_t)dst_desc.Width, (uint32_t)dst_desc.Height, (uint32_t)dst_desc.DepthOrArraySize,
                (uint32_t)dst_desc.MipLevels, (int)dst_desc.Format, (uint32_t)dst_desc.SampleDesc.Count,
                (uint32_t)src_state);
        }
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
    bool submitted = false;
    if (use_blit) {
        if (this->hud_blitter == nullptr) {
            this->hud_blitter = std::make_unique<HudBlitter>();
        }
        submitted = this->hud_blitter->blit(src, this->hud_ctx.textures[texture_index].texture,
                                            device, queue, src_state, src_srv);
    } else {
        copier->wait(INFINITE);
        // src is the engine backbuffer (PRESENT) during validation, or the UI-redirect RT (RENDER_TARGET).
        copier->copy(src, this->hud_ctx.textures[texture_index].texture,
                     src_state, D3D12_RESOURCE_STATE_RENDER_TARGET);
        copier->execute(queue);
        submitted = true;
    }

    XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(this->hud_handle, &release_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD xrReleaseSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }
    return submitted;
}

bool D3D12Component::OpenXR::copy_hud_delta(VR* vr, ID3D12Resource* final_src, ID3D12Resource* base_src,
                                            ID3D12Device* device, ID3D12CommandQueue* queue,
                                            D3D12_RESOURCE_STATES final_state,
                                            D3D12_RESOURCE_STATES base_state) {
    std::scoped_lock _{this->mtx};

    auto& openxr = vr->m_openxr;
    if (!this->hud_ready || this->hud_handle == XR_NULL_HANDLE || openxr == nullptr ||
        final_src == nullptr || base_src == nullptr) {
        return false;
    }
    if (openxr->frame_state.shouldRender != XR_TRUE || !openxr->frame_began) {
        return false;
    }
    if (this->hud_ctx.textures.empty() || this->hud_ctx.textures[0].texture == nullptr) {
        return false;
    }

    uint32_t texture_index{};
    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    auto result = xrAcquireSwapchainImage(this->hud_handle, &acquire_info, &texture_index);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD delta xrAcquireSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }

    XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait_info.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(this->hud_handle, &wait_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD delta xrWaitSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }

    if (this->hud_blitter == nullptr) {
        this->hud_blitter = std::make_unique<HudBlitter>();
    }
    const bool submitted = this->hud_blitter->blit_diff(
        final_src, base_src, this->hud_ctx.textures[texture_index].texture,
        device, queue, final_state, base_state);

    XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(this->hud_handle, &release_info);
    if (result != XR_SUCCESS) {
        spdlog::error("[VR] HUD delta xrReleaseSwapchainImage failed: {}", openxr->get_result_string(result));
        return false;
    }
    return submitted;
}

} // namespace vrmod
