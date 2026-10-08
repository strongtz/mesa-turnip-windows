# Mesa 提交组织

基线：`7a51c0f5326ee011b16a854499abf1db6f6c8b29`。保留完整上游 Git 历史，在 `main` 上追加以下提交。
构建使用 `mesa-revision.json` 中的完整 SHA，不依赖浮动分支。

| 提交 | 范围 |
|---|---|
| `49a45799867` | Windows Clang 原子操作与 ARM64 FP 状态 |
| `31c42dec680` | ARM64EC 架构识别和 xxhash |
| `bb0d27a6b5c` | Freedreno 公共代码与 IR3 的 Windows 编译兼容 |
| `85358ed54db` | IR3 磁盘缓存的可移植标识 |
| `c33c6dec502` | Microsoft ABI 下枚举位域的值保留 |
| `4435f79c873` | Turnip 同步与平台辅助代码 |
| `4972940f015` | clang-cl 可空弱入口函数 |
| `aee36f88326` | Vulkan 交换链状态的初始化与释放 |
| `534e8835f8e` | Win32 WSI 共享 GPU 缓冲区与呈现计时 |
| `dba8d7e527c` | Qualcomm WDDM 后端与能力开放 |
| `c00f23de340` | 对齐缓冲区复制的多行 2D 批处理 |
| `9ccec68b015` | WDDM 大块对齐复制的计算路径 |
| `c78e9baf503` | GPL 动态描述符缺失 shader variant 的处理 |
| `02983d680ef` | cube-compatible 图像的 MSRTSS 样本数 |
| `977157a41a4` | host-coherent 内存免缓存维护 |
| `da1ed0edf95` | 移除镜像遗留的过时 macOS CI |
| `16f87fbcc8e` | XML 头文件检查的 Windows 路径规范化 |

按范围拆分用于审阅和后续移植，不保证任意中间提交都具备完整的 Windows 构建和运行能力。
ARM64X 链接和 LLVM/Meson 兼容处理放在本仓库的构建脚本中，没有把生成文件加入 Mesa。

审阅全部移植差异：

```sh
git diff 7a51c0f5326ee011b16a854499abf1db6f6c8b29..16f87fbcc8e1aced8b2778e4df639762974f9e34
git log --reverse --oneline 7a51c0f5326ee011b16a854499abf1db6f6c8b29..16f87fbcc8e1aced8b2778e4df639762974f9e34
```

后续修复追加提交；更新构建锁定 SHA 后重新运行 CI 和本机硬件验证。不要把编译输出、原厂驱动或私有符号加入源码仓库。
