# Mesa Turnip for Windows ARM64 / ARM64X

This project connects the Mesa Turnip Vulkan userspace driver to Qualcomm's stock Windows KMD on Snapdragon X Elite.
Turnip/IR3 generates GPU commands and shaders; window presentation uses shared GPU buffers through D3D12/DXGI.

- [Mesa source and patch series](https://github.com/strongtz/mesa)
- [Build and packaging workflow](https://github.com/strongtz/mesa-turnip-windows/actions/workflows/build.yml)
- [Architecture and limitations](docs/architecture.md) · [Build instructions](docs/building.md)
- [Mesa patch organization](docs/patches.md)

This repository contains build scripts, test programs, and documentation. `mesa-revision.json` pins the source commit; builds do not automatically follow Mesa's main branch.
The current port targets the validated Adreno X1-85 configuration, with QCT private interface 4.50 and chip `0x60c512`.
This is an experimental port and has not completed Vulkan conformance certification.

## Usage

Download the `turnip-windows-<commit>` artifact from a successful Actions run. Extract it, then extract the included `turnip-win-arm64x.zip` and run:

```bat
run-with-turnip.cmd "C:\path\application.exe" [arguments]
```

For example:

```bat
run-with-turnip.cmd C:\data\FurMark_win64\furmark.exe --demo furmark-vk --width 1280 --height 720 --gpu-index 0
```

Some applications must start from their own directory. Change to that directory first, then invoke the launcher using its absolute path.
The launcher sets Vulkan ICD environment variables only for the child process. It does not replace the system driver or modify the registry.
The system must have a Vulkan loader and the Microsoft Visual C++ runtime installed.

| Package | Intended use |
|---|---|
| `turnip-win-arm64.zip` | Native ARM64 applications |
| `turnip-win-arm64x.zip` | ARM64 and x64 applications, using a single ARM64 + ARM64EC DLL |
| `*-symbols.zip` | Debug symbols for the corresponding driver |

ARM64X requires Windows on Arm and does not work on ordinary x64 PCs. Windows still handles compatibility execution for the x64 application itself, while the driver uses ARM64EC code.

## Building

On Windows ARM64, install Git, Python, Visual Studio 2026 with the C++ ARM64/ARM64EC toolchain, LLVM 22+, and Windows SDK 26100+, then run:

```powershell
python scripts/bootstrap.py
.venv/Scripts/python.exe scripts/build.py
.venv/Scripts/python.exe scripts/build_tests.py
.venv/Scripts/python.exe scripts/verify.py
.venv/Scripts/python.exe scripts/package.py
```

Packages are written to `dist/`. An optional proxy can be specified with `python scripts/bootstrap.py --proxy http://127.0.0.1:7890`.
If subsequent build steps need a proxy, set `HTTP_PROXY` / `HTTPS_PROXY` in the current terminal.

CI checks DLL loading, ICD negotiation, and entrypoint lookup for both process architectures. These checks on a virtual machine do not constitute GPU testing.
On a supported Snapdragon machine, also run:

```powershell
.venv/Scripts/python.exe scripts/verify.py --gpu --wsi
```

`--wsi` opens test windows and checks the pixels actually displayed. It requires an unlocked, visible desktop.

## Development

Mesa changes are split into commits covering build compatibility, common Vulkan fixes, the WDDM backend, WSI, copy optimizations, and ARM64EC support.
To update the source version, explicitly change `mesa-revision.json` and rerun CI and hardware regression tests.
Qualcomm driver binaries, private PDBs, the Vulkan SDK, and FurMark are not included and are not required to build the driver.

The scripts and tests in this repository use the MIT license. Mesa and downloaded dependencies retain their respective licenses; driver packages include Mesa's license documentation.
