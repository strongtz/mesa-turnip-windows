# Mesa patch organization

Base commit: `7a51c0f5326ee011b16a854499abf1db6f6c8b29`. The full upstream Git history is preserved, with the following commits added on `main`.
Builds use the full SHA recorded in `mesa-revision.json`, rather than a moving branch reference.

| Commit | Scope |
|---|---|
| `49a45799867` | Windows Clang atomics and ARM64 floating-point state |
| `31c42dec680` | ARM64EC architecture detection and xxhash |
| `bb0d27a6b5c` | Windows build compatibility for Freedreno common code and IR3 |
| `85358ed54db` | Portable identifiers for the IR3 disk cache |
| `c33c6dec502` | Preserve enum bitfield values under the Microsoft ABI |
| `4435f79c873` | Turnip synchronization and platform helpers |
| `4972940f015` | Nullable weak entrypoints with clang-cl |
| `aee36f88326` | Vulkan swapchain state initialization and cleanup |
| `534e8835f8e` | Win32 WSI shared GPU buffers and presentation timing |
| `dba8d7e527c` | Qualcomm WDDM backend and capability exposure |
| `c00f23de340` | Multiple-row 2D batching for aligned buffer copies |
| `9ccec68b015` | Compute path for large aligned buffer copies on WDDM |
| `c78e9baf503` | Handle missing shader variants for GPL dynamic descriptors |
| `02983d680ef` | MSRTSS sample counts for cube-compatible images |
| `977157a41a4` | Skip cache maintenance for host-coherent memory |
| `da1ed0edf95` | Remove the obsolete macOS CI inherited from the mirror |
| `16f87fbcc8e` | Normalize Windows paths in the XML header include check |

The commits are split by scope for review and future porting. Intermediate commits are not guaranteed to provide a complete Windows build or working driver.
ARM64X linking and LLVM/Meson compatibility handling live in this repository's build scripts. Generated files are not added to Mesa.

To review the full port:

```sh
git diff 7a51c0f5326ee011b16a854499abf1db6f6c8b29..16f87fbcc8e1aced8b2778e4df639762974f9e34
git log --reverse --oneline 7a51c0f5326ee011b16a854499abf1db6f6c8b29..16f87fbcc8e1aced8b2778e4df639762974f9e34
```

Add subsequent fixes as new commits. After updating the pinned source SHA, rerun CI and local hardware validation. Keep build output, vendor drivers, and private symbols out of the source repository.
