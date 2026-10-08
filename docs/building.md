# Building and packaging

## Toolchain

- Windows ARM64: Meson runs ARM64/ARM64EC compiler checks during configuration. These native build scripts currently do not support building directly on an x64 host.
- Visual Studio 2026 with C++ ARM64, ARM64EC, and x64 tools, plus LLVM 22+ tools: clang-cl, lld-link, llvm-lib, llvm-objcopy, and llvm-readobj.
- Windows SDK 10.0.26100 or newer, Python 3.12+, and Git.
- The scripts discover the toolchain through `vswhere` and `vcvarsarm64.bat`, without assuming a Community or Enterprise installation path.
- Python dependencies are pinned in `requirements.txt`. The winflexbison 2.5.25 download is checked against a SHA256 digest, and glslang is pinned to a specific commit.
- Mesa wrap dependencies use the versions and checksums recorded in the source tree. The first build needs access to GitHub and Meson WrapDB.

`bootstrap.py` creates separate `.venv` and `deps` directories in this repository. Existing tools can be reused:

```powershell
python scripts/bootstrap.py --glslang C:\tools\glslang --winflexbison C:\tools\winflexbison
.venv/Scripts/python.exe scripts/build.py --mesa C:\src\mesa --jobs 8
```

The specified Mesa working tree must be clean, and HEAD must match the pinned commit. The scripts do not reset existing source directories.

## ARM64X build process

1. Build ARM64 and ARM64EC separately, both using `/MD` and linking zlib statically.
2. Fix Meson 1.12.1's detection of the clang-cl `arm64ec` target. This change is limited to the local Python environment.
3. Resolve conflicts between LLVM 22 COFF weak symbols and ARM64EC AntiDependency aliases in separate copies of the object files. Only optional function references in Vulkan entrypoint tables are rewritten. Original Ninja object files are preserved, and unimplemented functions remain weak references.
4. Collect both sets of resolved linker inputs with `/linkreprofullpathrsp`, then merge them using `/machine:arm64x` and the two export definitions.
5. Check the PE machine type and CHPE metadata, then generate the ICD manifest, build information, and SHA256 checksums.
6. Load the driver from ARM64 and x64 processes to verify ICD negotiation and lookup of implemented and nonexistent entrypoints.

ARM64EC uses hybrid CRT/SDK libraries containing EC members from the ARM64 library directories. Simply replacing all library paths with x64 paths does not work.
These toolchain compatibility measures are implemented in `scripts/arm64ec.py`. Unknown C++ symbol forms cause the build to fail rather than silently produce incorrect entrypoint tables.

Reference: [Microsoft ARM64X build instructions](https://learn.microsoft.com/en-us/windows/arm/arm64x-build).

## GitHub Actions

The workflow uses `windows-11-arm` and runs on pushes, pull requests, or manual dispatch.
It pins the Mesa SHA, builds both architectures, generates ARM64/ARM64X driver and PDB packages, and uploads logs and verification reports.
The workflow requires neither private PDBs nor GitHub secrets. Its permissions are limited to `contents: read`.

The hosted runner does not have this machine's Qualcomm GPU/KMD, so CI does not run `--gpu` or `--wsi`.
After downloading artifacts, graphics, compute, synchronization, memory stress, and CTS validation still need to run on real hardware.

## Directories

- `build/`: Meson/Ninja output, copies of ARM64EC entrypoint objects, linker response files, and test executables.
- `out/`: Uncompressed packages, build information, and verification reports.
- `dist/`: Distribution ZIP files.
- `logs/`: Test output.
- `tests/`: Compute, copy, synchronization, BDA, and window tests with explicit output checks. Memory stress tests are run separately by hand.

Generated directories are not committed. PDBs are packaged separately and must be paired with DLLs from the same build.
