# 构建与交付

## 工具链

- Windows ARM64：Meson 配置阶段会运行 ARM64/ARM64EC 编译检查；目前不支持在 x64 主机上直接使用这套原生构建脚本。
- Visual Studio 2026，C++ ARM64、ARM64EC、x64 工具，以及 LLVM 22+ 的 clang-cl、lld-link、llvm-lib、llvm-objcopy、llvm-readobj。
- Windows SDK 10.0.26100 或更高；Python 3.12+；Git。
- 脚本通过 `vswhere` 和 `vcvarsarm64.bat` 发现工具链，不依赖 Community/Enterprise 安装路径。
- Python 依赖固定在 `requirements.txt`；winflexbison 2.5.25 下载后校验 SHA256；glslang 固定到具体提交。
- Mesa wrap 依赖使用源码中已有的版本和校验和。首次构建需要访问 GitHub 和 Meson WrapDB。

`bootstrap.py` 在本仓库创建独立 `.venv` 和 `deps`。已有工具可以复用：

```powershell
python scripts/bootstrap.py --glslang C:\tools\glslang --winflexbison C:\tools\winflexbison
.venv/Scripts/python.exe scripts/build.py --mesa C:\src\mesa --jobs 8
```

指定的 Mesa 工作树必须干净，且 HEAD 必须等于锁定提交。脚本不会重置已有源码目录。

## ARM64X 生成过程

1. 独立构建 ARM64 和 ARM64EC，均使用 `/MD`，zlib 静态链接。
2. 修正 Meson 1.12.1 对 clang-cl `arm64ec` 目标的识别；修改仅限本地 Python 环境。
3. 在独立副本中处理 LLVM 22 COFF 弱符号与 ARM64EC AntiDependency 别名的冲突。仅改写 Vulkan 入口表的可选函数引用；保留原始 Ninja 目标文件，未实现函数仍为弱引用。
4. 用 `/linkreprofullpathrsp` 收集两套已解析的链接输入，使用 `/machine:arm64x` 和两套导出定义合并。
5. 检查 PE machine、CHPE 元数据，生成 ICD manifest、构建信息和 SHA256 清单。
6. 分别从 ARM64 和 x64 加载驱动，验证 ICD 协商以及已实现/不存在的入口查询。

ARM64EC 使用 ARM64 目录下含 EC 成员的混合 CRT/SDK 库。不能简单把所有库目录替换成 x64。
这些工具链兼容措施集中在 `scripts/arm64ec.py`，未知 C++ 符号形式会导致构建失败，而不是静默产生错误入口表。

参考：[Microsoft ARM64X 构建说明](https://learn.microsoft.com/en-us/windows/arm/arm64x-build)。

## GitHub Actions

工作流使用 `windows-11-arm`，触发条件是本仓库 push、pull request 或手动运行。
固定 Mesa SHA，构建两套代码，生成 ARM64/ARM64X 包和 PDB 包，并上传日志与验证报告。
工作流不需要私有 PDB，也不需要 GitHub secret。权限仅为 `contents: read`。

托管 runner 没有本机 Qualcomm GPU/KMD，所以 CI 不运行 `--gpu` 或 `--wsi`。
下载构建产物后，仍需在真实设备上进行图形、计算、同步、内存压力和 CTS 验证。

## 目录

- `build/`：Meson/Ninja 输出、ARM64EC 入口表副本、链接响应文件、测试可执行文件。
- `out/`：未压缩包、构建信息、验证报告。
- `dist/`：发布 ZIP。
- `logs/`：测试输出。
- `tests/`：有确定输出检查的计算、复制、同步、BDA 和窗口测试；内存压力测试另行手动运行。

上述生成目录均不提交。PDB 独立打包，使用时和相同构建的 DLL 配对。
