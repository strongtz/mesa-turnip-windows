# Architecture and limitations

## Driver path

```text
Vulkan application (ARM64 or x64)
  -> Windows Vulkan loader
  -> Turnip ARM64X ICD (ARM64 / ARM64EC views)
  -> Mesa Vulkan runtime, Turnip, IR3
  -> Qualcomm WDDM backend
  -> D3DKMT / stock Qualcomm KMD
  -> Adreno X1-85
```

Turnip generates Adreno commands for rendering and compute. D3D12 provides window presentation interoperability; it does not translate Vulkan shaders.

## WDDM

`tu_knl_wddm.cc` handles adapter/device initialization, native context submission, GPU VA allocation, residency, buffer reclamation, binary/timeline synchronization, timestamp calibration, and budget queries.
The backend currently requires QCT private interface version 4.50 and chip `0x60c512`. Support for other Qualcomm driver versions or Adreno models is not claimed.
Locally provided symbol information was used during reverse engineering. The build does not depend on vendor PDBs, and the repository does not distribute them.

The backend supports two submission queues, 47-bit GPU virtual addresses, BDA capture/replay, and deferred resource reclamation.
The per-allocation limit is `0xffff0000` bytes, which is distinct from the total capacity across multiple allocations.
Active allocations remain resident; there is no independent eviction or oversubscription manager yet.

## Presentation

```text
Turnip render image
  -> GPU copy from image to linear buffer
  -> DEFAULT-heap buffer shared with D3D12
  -> D3D12 CopyTextureRegion
  -> DXGI back buffer
  -> Present / Windows display system
```

The current path has two intermediate GPU copies. The shared buffer refers to the same memory and does not require an additional CPU readback/upload.
Native monitored fences connect Turnip submissions to the D3D12 queue. A D3D12 completion fence protects buffer and command allocator reuse.
The test program uses GDI pixel reads only for verification; the driver's default presentation path does not copy whole frames through the CPU.

Rendering directly into presentable textures will require further work on DXGI texture layouts, compression metadata, imports, and state coordination.
The share of FurMark frame time spent on these two copies has not been measured.

## Available features and remaining work

Enabled features include Vulkan 1.3, BDA, timeline synchronization, descriptor indexing/buffers, GPL, geometry/tessellation, Transform Feedback, several dynamic states, Win32 presentation, present ID/wait, and some presentation timing support.
Hardware-dependent features retain common Turnip capability checks; the Windows backend uses an explicit allowlist to select which capabilities to expose.

Remaining priorities:

1. Share DXGI images directly to reduce presentation copies.
2. Expose application-facing Win32 external memory, semaphore, and fence import/export. Internal presentation interoperability does not imply support for public extensions.
3. Support sparse resources, residency eviction, and oversubscription policies.
4. Enable and test more optional extensions, such as maintenance5-9 and robustness2.
5. Add queue priorities, performance counters, and device fault/recovery diagnostics.
6. Expand Qualcomm KMD/GPU version coverage and ongoing CTS/application regression testing.

ARM64X changes the CPU-side ABI and loading behavior. It does not automatically provide the GPU or system integration features listed above.
