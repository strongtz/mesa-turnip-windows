# 验证记录

## GitHub Actions 产物验证（2026-10-08）

[成功运行](https://github.com/strongtz/mesa-turnip-windows/actions/runs/37739616030)，构建脚本提交 `dac9199b068b7b01c76c60c8ace6f8567e8954d0`，Mesa `16f87fbcc8e1aced8b2778e4df639762974f9e34`。

- `windows-11-arm` 从干净环境下载依赖，编译 ARM64/ARM64EC，合并 ARM64X，编译两种测试程序，并通过三组 DLL ABI 检查。
- 上传了 ARM64、ARM64X 运行包及各自的 PDB 包；总产物约 31 MB。
- 下载上述 CI 产物到 Snapdragon X Elite 后，四个 ZIP 的完整性和全部 SHA256 清单条目通过。
- 对下载包运行同一套真实 GPU/窗口检查：三个包/架构组合共 24 项全部通过，包括每组 18 个交换链、360 次显示像素检查。
- CI ARM64 DLL：`525f738db065e4e710ab666530414c002257b803b6bae602e76c99ebe78e95db`。
- CI ARM64X DLL：`e9669b35d0c3dd4f7b3a5e82ca30af4eb9cd0f4a8b1b192bf156032c434db4c7`。

真实 GPU 检查是在下载后于本机完成的，不是在 GitHub 托管 runner 上完成的。

## 独立脚本的干净构建（2026-10-08）

Mesa `16f87fbcc8e1aced8b2778e4df639762974f9e34`，本机 LLVM 22.1.3。
使用本仓库的新 `.venv`、Meson 构建目录和链接脚本重新构建；复用了已安装的 winflexbison/glslang。

- ARM64 包的原生进程、ARM64X 包的 ARM64/x64 进程：共 24 项检查全部通过。
- 每种组合均检查装载/入口、计算、同步、BDA、复制、复制前后计算状态、类型运算及窗口像素。
- 四个 ZIP 的完整性及所有 SHA256 清单条目均通过校验。
- ARM64 DLL：`5f77880d521be94e0b4eda0d9211abf08baab09faa259b509ca61072fafd7295`。
- ARM64X DLL：`5186e69256e04003db6dc50393679e5b983cb0030951d2e15cea236642c4799c`。

这是本机构建的记录；CI 使用托管工具链，生成的二进制哈希可能不同。

## Snapdragon X Elite 本机记录（2026-10-08）

整理提交前的 ARM64X 构建已在 Adreno X1-85 上验证：

- 原生 ARM64 和 x64：GPU 计算、跨队列同步、时间线值、BDA/计算状态恢复全部通过。
- 两种架构的窗口测试各覆盖 18 组交换链、360 个显示像素检查，包含格式、sRGB、呈现模式、resize/recreate 和销毁。
- FurMark x64，`furmark-vk`，1280×720，GPU 0：最终 20 秒运行 1099 帧，报告 min/avg/max 45/55/58 FPS，正常退出、stderr 为空。
- 另一次 60 秒 FurMark 运行完成 3329 帧，平均 55 FPS；实际窗口内容已检查。

这些是历史本机测量，不是所有未来 CI 构建的性能承诺。
提交整理保留了原有 46 个修改/新增源码文件的内容；另行移除了上游镜像中已失效的 macOS GitHub 工作流，并修复了 XML 检查脚本的 Windows 路径比较。

## 较早 ARM64 验证范围

此前按功能分组运行过 Vulkan CTS，包括计算布局、同步/BDA、内存、描述符/GPL、光栅化/纹理、几何/细分/XFB、现代绘制以及 Win32 WSI/计时。
这些是选定测试组的回归，不是全量 CTS 一致性认证。unsupported 不计作通过。
曾完成 5.4 GiB 多缓冲区 GPU 读写与分配耗尽后恢复检查；没有通过人为 GPU 故障验证设备丢失恢复。

## 可重复检查

```powershell
# 任意 Windows ARM64 runner：检查 ABI、导出、入口表，不依赖 Qualcomm GPU
.venv/Scripts/python.exe scripts/verify.py

# 支持的 Snapdragon 机器：检查真实 GPU 计算、复制、同步和 BDA
.venv/Scripts/python.exe scripts/verify.py --gpu

# 已解锁桌面：追加显示内容和交换链生命周期检查
.venv/Scripts/python.exe scripts/verify.py --gpu --wsi
```

每条记录保存 DLL SHA256、程序架构、退出码和日志文件名到 `out/verification.json`。
`scripts/package.py` 将报告与精确 Mesa SHA 放入 ZIP。CI 上传日志以便区分编译成功、ABI 检查成功和真实 GPU 测试成功。
