# FH5 color issue - offline report

Scope: offline review only. FH5 was not launched or attached. No implementation files were edited.

## Ranked findings

### 1. OpenXR swapchain format is hardcoded to `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`

Likelihood: high.

Evidence:

- `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:234-243` reads and logs the real FH5 backbuffer `desc.Format`.
- `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:279-287` ignores that stored format and always creates OpenXR swapchains as `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`.
- `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:420-425` copies the engine backbuffer directly into the OpenXR image with `CopyResource`.

Why this matters:

`CopyResource` requires compatible resources and does no gamma conversion. If FH5 is presenting `B8G8R8A8_UNORM`, `R8G8B8A8_UNORM`, `R10G10B10A2_UNORM`, an HDR format, or linear data, then hardcoding `R8G8B8A8_UNORM_SRGB` can produce wrong channel interpretation, wrong gamma, or undefined/incompatible copy behavior. The comment assumes FH5's backbuffer is sRGB-encoded 8-bit data, but the code only logs the backbuffer format; it does not enforce/verify that assumption.

Smallest confirmation:

Check `FH5VR.log` for:

```text
[VR] D3D12 backbuffer <w>x<h> format <n>
```

Then compare `<n>` against the OpenXR swapchain format currently hardcoded at line 287. If `<n>` is not `29` (`DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`) or at least the same typeless family with the intended gamma semantics, this is the first fix target.

Smallest code probe:

Log both values in `create_swapchains()`:

```cpp
spdlog::info("[VR] color path: backbuffer_format={} xr_swapchain_format={}",
    (int)vr->m_d3d12_backbuffer_format_or_accessor, (int)swapchain_create_info.format);
```

If no accessor exists, add a private getter on `D3D12Component` for `m_backbuffer_format`.

### 2. The code does not enumerate runtime-supported OpenXR swapchain formats

Likelihood: medium-high.

Evidence:

- `E:\Github\vrframework\include\vr\runtimes\OpenXRDispatch.hpp:61-65` declares swapchain create/image/acquire/wait/release functions, but not `xrEnumerateSwapchainFormats`.
- `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:279-300` creates swapchains directly with the hardcoded format. There is no supported-format selection path.

Why this matters:

OpenXR runtimes are allowed to expose a set of supported D3D formats. If the runtime accepts the hardcoded `_SRGB` format but expects different compositor behavior than SimXR, or if a real HMD runtime prefers a different format, color can be wrong even if swapchain creation succeeds.

Smallest confirmation/fix:

Add `PFN_xrEnumerateSwapchainFormats` to the dispatch layer, enumerate formats before creating swapchains, log them, and choose in this order:

1. exact match to the game backbuffer if compatible;
2. `_SRGB` only when the source is known sRGB-encoded display data;
3. linear `R8G8B8A8_UNORM` or a shader blit path when conversion is required.

### 3. HDR/non-8-bit conversion path is intentionally omitted

Likelihood: medium, high if FH5 is running HDR or a 10-bit output mode.

Evidence:

- `E:\Github\vrframework\include\mods\vr\D3D12Component.hpp:8-14` says the REFramework HDR-to-8-bit SpriteBatch conversion was omitted.
- `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:148-150` repeats that non-8-bit backbuffers are not converted and assumes FH5's validated backbuffer is 8-bit.

Why this matters:

If FH5 is configured for HDR, Windows HDR, or a 10-bit swapchain, a direct copy into an 8-bit XR swapchain cannot preserve color. It needs a shader/tonemap blit or a matching XR swapchain format.

Smallest confirmation:

Disable FH5/Windows HDR and check whether the color issue disappears. Also log `desc.Format` on startup and after resize.

### 4. Channel-order mismatch is possible if FH5 backbuffer is BGRA

Likelihood: medium.

Evidence:

- The dummy hook swapchain initially uses `DXGI_FORMAT_B8G8R8A8_UNORM` in `E:\Github\vrframework\src\hooks\D3D12Hook.cpp:58`.
- The XR swapchain is always `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB` in `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:287`.
- Direct `CopyResource` is used at `E:\Github\vrframework\src\mods\vr\D3D12Component.cpp:95` and `:423-425`.

Why this matters:

If the real FH5 swapchain is BGRA and the OpenXR swapchain is RGBA, the direct copy path is not a color conversion path. Even when a runtime/device allows a copy across related formats, it will not swizzle channels for us.

Smallest confirmation:

Log real `desc.Format`. If it is `87` (`B8G8R8A8_UNORM`) or `91` (`B8G8R8A8_UNORM_SRGB`), route through a shader blit that samples source with the correct SRV format and writes to the XR image with the intended RTV format.

## Best next action

First add a bounded diagnostic log:

```text
backbuffer format, width, height
enumerated OpenXR swapchain formats
chosen XR format
whether source is treated as sRGB or linear
```

Then choose the XR swapchain format from the real backbuffer/runtime format list instead of hardcoding `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`. If the source and destination are not copy-compatible with the intended gamma/channel semantics, replace `CopyResource` with a one-pass shader blit/conversion.
