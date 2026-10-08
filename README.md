# Mesa Turnip for Windows ARM64 / ARM64X

在 Snapdragon X Elite 上，将 Mesa Turnip Vulkan 用户态驱动接到 Qualcomm 原厂 Windows KMD。
GPU 命令和着色器由 Turnip/IR3 生成；窗口呈现通过共享 GPU 缓冲区接入 D3D12/DXGI。

- [Mesa 源码及分拆提交](https://github.com/strongtz/mesa)
- [构建和打包工作流](https://github.com/strongtz/mesa-turnip-windows/actions/workflows/build.yml)
- [架构与当前限制](docs/architecture.md) · [构建说明](docs/building.md) · [测试记录](docs/validation.md)

本仓库保存构建脚本、测试程序和文档。`mesa-revision.json` 固定源码提交；不会自动跟随 Mesa 主分支。
当前仅针对已验证的 Adreno X1-85、QCT 私有接口 4.50 / chip `0x60c512`。
这是实验性移植，尚未完成 Vulkan 一致性认证。

## 使用

从 Actions 的成功运行下载 `turnip-win-arm64x.zip`，解压后运行：

```bat
run-with-turnip.cmd "C:\path\application.exe" [arguments]
```

例如：

```bat
run-with-turnip.cmd C:\data\FurMark_win64\furmark.exe --demo furmark-vk --width 1280 --height 720 --gpu-index 0
```

某些程序需要在自身目录启动；可先切换到程序目录，再用启动脚本的绝对路径调用。
脚本仅为子进程设置 Vulkan ICD 环境变量，不替换系统驱动、不修改注册表。
系统需有 Vulkan loader 和 Microsoft Visual C++ 运行库。

| 包 | 使用对象 |
|---|---|
| `turnip-win-arm64.zip` | 原生 ARM64 应用 |
| `turnip-win-arm64x.zip` | ARM64 和 x64 应用，共用一个 ARM64 + ARM64EC DLL |
| `*-symbols.zip` | 对应驱动的调试符号 |

ARM64X 需要 Windows on Arm；不适用于普通 x64 PC。x64 应用自身仍由 Windows 处理兼容执行，驱动使用 ARM64EC 代码。

## 构建

在 Windows ARM64 上安装 Git、Python、Visual Studio 2026 的 C++ ARM64/ARM64EC 工具链、LLVM 22+ 和 Windows SDK 26100+，然后：

```powershell
python scripts/bootstrap.py
.venv/Scripts/python.exe scripts/build.py
.venv/Scripts/python.exe scripts/build_tests.py
.venv/Scripts/python.exe scripts/verify.py
.venv/Scripts/python.exe scripts/package.py
```

输出位于 `dist/`。可选代理：`python scripts/bootstrap.py --proxy http://127.0.0.1:7890`。
后续构建如需代理，请在当前终端设置 `HTTP_PROXY` / `HTTPS_PROXY`。

CI 验证两种进程架构的 DLL 加载、ICD 协商和入口查询，不把虚拟机上的检查称为真实 GPU 测试。
在受支持的 Snapdragon 机器上追加运行：

```powershell
.venv/Scripts/python.exe scripts/verify.py --gpu --wsi
```

`--wsi` 会打开测试窗口并检查实际显示的像素，需要已解锁、可见的桌面。

## 开发

Mesa 改动按编译兼容、通用 Vulkan 修复、WDDM 后端、WSI、复制优化和 ARM64EC 支持分开提交。
更新源码版本时显式修改 `mesa-revision.json`，重新执行 CI 和硬件回归。
不包含高通驱动二进制、私有 PDB、Vulkan SDK 或 FurMark；构建不需要这些文件。

本仓库脚本与测试使用 MIT 许可证。Mesa 及下载依赖保留各自许可证，驱动包附带 Mesa 许可证说明。
