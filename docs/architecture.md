# 实现与差距

## 路径

```text
Vulkan 应用（ARM64 或 x64）
  → Windows Vulkan loader
  → Turnip ARM64X ICD（ARM64 / ARM64EC 视图）
  → Mesa Vulkan runtime、Turnip、IR3
  → 新增 Qualcomm WDDM 后端
  → D3DKMT / 原厂高通 KMD
  → Adreno X1-85
```

渲染和计算由 Turnip 生成 Adreno 命令；D3D12 用于窗口呈现互操作，不负责翻译 Vulkan 着色器。

## WDDM

`tu_knl_wddm.cc` 负责适配器/设备初始化、原生上下文提交、GPU VA 分配、驻留、缓冲区回收、二进制/时间线同步、时间戳校准和预算查询。
QCT 私有接口目前要求版本 4.50 和 chip `0x60c512`，不声称支持任意高通驱动版本或 Adreno 型号。
逆向研究阶段参考了本机提供的符号信息；构建和仓库均不依赖或分发原厂 PDB。

已支持两个提交队列、47 位 GPU VA、BDA capture/replay 和延迟资源回收。
单次分配上限为 `0xffff0000` 字节，和多分配总容量限制分开处理。
活跃分配保持驻留；尚无自主驱逐/超额分配管理器。

## 呈现

```text
Turnip 渲染图像
  → GPU 图像到线性缓冲区复制
  → 与 D3D12 共享的 DEFAULT-heap 缓冲区
  → D3D12 CopyTextureRegion
  → DXGI 后台图像
  → Present / Windows 显示系统
```

当前有两段中间 GPU 搬运。共享缓冲区是同一份内存，不另做 CPU 读回/上传。
原生 monitored fence 连接 Turnip 提交和 D3D12 队列；D3D12 完成 fence 保护缓冲区与命令分配器重用。
测试程序的 GDI 像素读取只用于验证，驱动默认呈现路径不做 CPU 整帧复制。

后续需要完善 DXGI 纹理的布局、压缩元数据、导入和状态协同，才有机会直接渲染到可呈现纹理。
尚未测量这两段复制在 FurMark 总帧时间中所占比例。

## 已开放与未完成

已开放 Vulkan 1.3、BDA、时间线同步、描述符索引/缓冲区、GPL、几何/细分、Transform Feedback、多项动态状态，以及 Win32 呈现、present ID/wait 与部分呈现计时。
硬件相关特性保持通用 Turnip 的能力判断；Windows 后端通过明确列表选择暴露范围。

剩余重点：

1. 直接共享 DXGI 图像以减少呈现复制。
2. 面向应用的 Win32 外部内存、信号量、fence 导入导出；内部呈现互操作不等于公开扩展支持。
3. 稀疏资源、驻留驱逐和超额分配策略。
4. 更完整的可选扩展开放与测试，如 maintenance5–9、robustness2。
5. 队列优先级、性能计数器、设备故障与恢复诊断。
6. 扩大 Qualcomm KMD/GPU 版本覆盖和长期 CTS/应用回归。

ARM64X 改变 CPU 端 ABI 与装载方式，不自动补齐上述 GPU 或系统集成功能。
