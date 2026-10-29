# i.MX6ULL 移植准备指南

本文档说明 IMX6ULL-CarTerminal 从当前 x86-64 Linux 主机版本迁移到具体 i.MX6ULL 板卡前必须确认的输入、建议的交叉编译流程、部署布局和板端验收项目。

## 当前验证边界

当前 V2.2 已在 Ubuntu 24.04 / WSL2 的 x86-64 Linux 用户空间完成 Debug、Release、CTest、ASan/UBSan 和 Valgrind 验证。项目只使用 C++17、标准线程、文件系统、文件流和少量 POSIX 时间函数。

截至 2026-10-29，当前主机没有检测到以下常见命令：

```text
arm-linux-gnueabihf-g++
arm-linux-gnueabi-g++
arm-none-linux-gnueabihf-g++
aarch64-linux-gnu-g++
```

因此尚未完成 ARM 交叉编译、目标链接、板端部署或实机运行验证。ThreadSanitizer 在当前 WSL2 环境会因 `unexpected memory mapping` 失败，也不能作为已通过项目记录。

## 移植前必须确定的目标输入

不要只根据“i.MX6ULL”芯片名称选择工具链。开始交叉编译前，需要从实际板卡、BSP 或系统镜像提供方确认：

1. 板卡厂商、产品型号、硬件版本和 i.MX6ULL 具体料号。
2. U-Boot、Linux 内核版本、设备树来源及其配置。
3. 根文件系统来源：厂商 BSP、Yocto、Buildroot、Debian 或其他方案。
4. 目标 ABI：32 位 ARM EABI、软浮点或硬浮点，以及编译器 target triple。
5. C/C++ 运行库组合：glibc 或 musl、对应 libstdc++、libgcc 和动态链接器路径。
6. 与目标镜像完全匹配的 sysroot，至少包含标准 C++ 头文件、目标库、pthread 和启动文件。
7. BSP 推荐的 CPU/FPU/浮点 ABI 选项。不要在通用工程中猜测 `-march`、`-mfpu` 或 `-mfloat-abi`。
8. 部署用户、只读/可写文件系统布局，以及日志与 CSV 的持久化位置和容量限制。
9. 板端是否挂载 `/proc`、是否支持 POSIX threads、系统时钟来源及校时方式。

i.MX6ULL 通常是 32 位 ARMv7-A/Cortex-A7 平台，但最终 ABI、编译选项和运行库必须以所用 BSP/镜像为准。

## 工程依赖与可移植性检查

当前源码没有 x86-64 内联汇编、SSE/AVX 指令或固定指针宽度假设。车辆数据使用明确的 `double` 和 `std::size_t`；序列化采用文本 CSV，不把内存布局直接写入文件。

需由目标环境提供：

- C++17 编译器及 libstdc++；
- POSIX 线程支持，供 `std::thread`、mutex 和 condition_variable 使用；
- C++17 filesystem 支持；
- 可写的日志和 CSV 目录；
- `localtime_r`、`gmtime_r` 和正常工作的系统墙钟。

模拟采集通过 `MonitoringService::AcquireFunction` 注入。硬件移植时可把真实适配器的 `readSample()` 绑定到该回调，无需修改状态判断、日志、CSV 或线程调度。除非实际出现多个复杂数据源和生命周期需求，否则无需先引入虚基类层次。

## 工具链模板

仓库提供：

```text
cmake/toolchains/imx6ull-linux-example.cmake
```

该文件不会猜测工具链前缀、ABI 或 sysroot。缺少输入时会主动终止配置。

示例命令中的路径必须替换为 BSP 的真实路径：

```bash
cmake -S . -B build-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/imx6ull-linux-example.cmake \
  -DIMX6ULL_CXX_COMPILER=/opt/vendor-sdk/bin/<target>-g++ \
  -DIMX6ULL_SYSROOT=/opt/vendor-sdk/sysroots/<target> \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=OFF

cmake --build build-arm --verbose
DESTDIR="$PWD/stage-arm" cmake --install build-arm
```

只有当编译器、链接器和 sysroot 全部匹配目标系统，且生成文件经 `file`/`readelf` 确认为目标 ARM ELF 后，才能记录为交叉编译成功。

不要用主机 `strip` 处理目标文件；如需裁剪符号，应使用工具链配套的 target `strip`，并保留一份带调试信息的构建产物。

## 安装布局

使用 `CMAKE_INSTALL_PREFIX=/usr` 和 GNUInstallDirs 的常见目标布局为：

```text
/usr/bin/car_terminal
/etc/imx6ull-car-terminal/default.conf
/usr/share/doc/imx6ull-car-terminal/README.md
/usr/share/doc/imx6ull-car-terminal/IMX6ULL_PORTING_GUIDE.md
/var/log/imx6ull-car-terminal/car_terminal.log       # 首次运行创建
/var/lib/imx6ull-car-terminal/vehicle_data.csv       # 首次运行创建
```

实际目录会受到 BSP 的 GNUInstallDirs 设置影响。安装版配置使用相对路径连接配置目录与 local-state 目录，因此也支持非 `/usr` 的测试前缀。

部署前必须确保运行用户能够创建或写入日志和 CSV 目录。生产系统还应配置日志轮转、存储容量和掉电恢复策略。

## 部署和运行

将 `stage-arm` 中的文件按相同绝对路径复制到目标根文件系统，或交给 BSP 的镜像/包系统处理。不要把主机构建目录、备份、测试日志或 x86-64 二进制复制到目标板。

板端运行示例：

```bash
/usr/bin/car_terminal \
  --config /etc/imx6ull-car-terminal/default.conf
```

在接入真实硬件之前，仍应先运行模拟配置，确认 C++ 运行库、线程、文件权限、时钟和进程退出正常。

## 板端验收清单

- `file` 和 `readelf -h` 显示目标 ARM 架构、正确位数和 ABI。
- `readelf -l` 中的动态链接器在目标根文件系统实际存在。
- `ldd` 或等效检查没有缺失的 libstdc++、libgcc、libc 或 pthread 依赖。
- `--help`、`--version` 和配置错误退出码正确。
- 10 次模拟采样完整，生产者和消费者能正常退出。
- 日志和 CSV 目录权限正确，重启后行为符合预期。
- 系统时间未同步时的时间戳行为被接受或有明确处理策略。
- 连续运行和磁盘空间不足时不会造成失控增长或死循环。
- 进程收到服务管理器停止请求时的策略已定义；当前版本尚未实现信号驱动的优雅停止。
- 真正的数据源适配器有超时、错误传播、断线重试和输入范围校验。

## 明确不在当前范围内的内容

- 未定义或实现任何 CAN 报文、车辆 DBC、诊断协议或真实传感器格式。
- 未声称已通过 i.MX6ULL 实机测试。
- 未选择 Yocto、Buildroot、Debian 或厂商 BSP；这些必须由目标项目决定。
- 未提供硬实时保证或功能安全认证。
