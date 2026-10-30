# i.MX6ULL 移植准备指南

本文档说明 IMX6ULL-CarTerminal 从当前 x86-64 Linux 主机版本迁移到具体 i.MX6ULL 板卡前必须确认的输入、建议的交叉编译流程、部署布局和板端验收项目。

## 当前验证边界

V2.2 已在 Ubuntu 24.04 / WSL2 的 x86-64 Linux 用户空间完成主机端验证。V2.3 增加安全信号退出与 CI 配置；V2.4 增加 Linux SocketCAN；V2.5 增加有限次数与连续监控；V2.6 增加大小轮转和可选 systemd 部署模板。本地验证结果以 README 和对应交付报告为准，GitHub Actions 在实际运行前不能标记为通过。

V2.4 当前主机没有 `vcan0`，也没有 `candump`/`cansend`，因此 vcan 测试被测试框架明确标记为跳过。纯解析、接收器替身、超时/停止协作和不存在接口初始化失败可以在不依赖 CAN 硬件时验证；这些结果不能替代真实 socket、物理总线或目标板验证。

截至 2026-10-30，当前主机没有检测到以下常见命令：

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
- Linux SocketCAN UAPI 头文件和启用了 CAN 子系统的目标内核（仅在使用 `data_source=socketcan` 时需要）；
- 可写的日志和 CSV 目录；
- 支持同一文件系统内原子 rename 的持久化目录，供大小轮转使用；
- `localtime_r`、`gmtime_r` 和正常工作的系统墙钟。

模拟采集和 `SocketCanDataSource` 都通过 `MonitoringService::AcquireFunction` 注入，无需修改状态判断、日志、CSV 或线程调度。V2.5 的连续模式只改变监控循环边界，不改变采集接口。未来其他硬件适配器也应保持这条边界；除非实际出现多个复杂数据源和生命周期需求，否则无需先引入虚基类层次。

停止谓词同样通过回调注入。SocketCAN 接收器使用有限 `poll` 超时并在每次等待后检查停止请求；其他真实 `readSample()` 也必须使用超时或可取消 I/O，避免永久阻塞导致连续模式无法及时响应 SIGINT/SIGTERM。

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
/etc/imx6ull-car-terminal/systemd.conf
/etc/imx6ull-car-terminal/socketcan-synthetic-example.conf
/usr/lib/systemd/system/imx6ull-car-terminal.service # 仅 systemd 系统
/usr/share/doc/imx6ull-car-terminal/README.md
/usr/share/doc/imx6ull-car-terminal/IMX6ULL_PORTING_GUIDE.md
/usr/share/doc/imx6ull-car-terminal/SOCKETCAN_VIRTUAL_PLAN.md
/var/log/imx6ull-car-terminal/car_terminal.log       # 首次运行创建
/var/lib/imx6ull-car-terminal/vehicle_data.csv       # 首次运行创建
```

实际目录会受到 BSP 的 GNUInstallDirs 设置影响。安装版配置使用相对路径连接配置目录与 local-state 目录，因此也支持非 `/usr` 的测试前缀。

部署前必须确保运行用户能够创建或写入日志和 CSV 目录。V2.6 已提供进程内大小轮转和归档保留上限；目标系统仍需验证磁盘容量、掉电恢复和文件系统耐久性。

## 部署和运行

将 `stage-arm` 中的文件按相同绝对路径复制到目标根文件系统，或交给 BSP 的镜像/包系统处理。不要把主机构建目录、备份、测试日志或 x86-64 二进制复制到目标板。

板端运行示例：

```bash
/usr/bin/car_terminal \
  --config /etc/imx6ull-car-terminal/default.conf
```

在接入真实硬件之前，仍应先运行模拟配置，确认 C++ 运行库、线程、文件权限、时钟和进程退出正常。

若目标发行版使用 systemd，可使用仓库的 unit 和安装脚本；默认服务以 `car-terminal` 非 root 用户运行并使用连续模拟配置。只有 SocketCAN 模式才按需安装 `CAP_NET_RAW` drop-in。非 systemd BSP 应把相同的 SIGTERM、重启、用户和目录权限策略转换为其 init 机制。详细步骤见 [SYSTEMD_DEPLOYMENT.md](SYSTEMD_DEPLOYMENT.md)。

切换 SocketCAN 前还必须由板卡/BSP 文档确认 CAN 控制器驱动、设备树、管脚复用、收发器、电气终端、位时序和接口启动方式。项目只接受已经存在且处于可用状态的 Linux CAN 网络接口；它不会加载内核模块、创建设备或配置波特率。示例映射只用于合成测试，不得部署为车辆协议。

## 板端验收清单

- `file` 和 `readelf -h` 显示目标 ARM 架构、正确位数和 ABI。
- `readelf -l` 中的动态链接器在目标根文件系统实际存在。
- `ldd` 或等效检查没有缺失的 libstdc++、libgcc、libc 或 pthread 依赖。
- `--help`、`--version` 和配置错误退出码正确。
- 10 次模拟采样完整，生产者和消费者能正常退出。
- 连续模拟模式能越过 `sample_count` 持续运行，并分别由 SIGINT/SIGTERM 安全停止。
- 信号停止后 CSV 行数、处理日志数量和最后样本号一致，两个线程均已连接。
- 日志和 CSV 目录权限正确，重启后行为符合预期。
- 系统时间未同步时的时间戳行为被接受或有明确处理策略。
- 评估连续运行的日志/CSV 增长、磁盘空间不足、轮转、掉电恢复和最长预期运行时间。
- 用小阈值触发多次轮转，确认 `.1` 为最新归档、保留数量正确且每个 CSV 都有表头。
- 若使用 systemd，确认非 root 用户、目录所有权、SIGTERM 排空、异常重启和可选 SocketCAN capability。
- SIGINT/SIGTERM 停止会排空已入队数据、刷新文件并以约定状态码退出。
- 真正的数据源适配器有超时、错误传播、断线重试和输入范围校验。
- SocketCAN 接口名称、标准/扩展帧过滤、接收超时、错误帧/RTR/短帧处理均在目标内核上重新验证。
- 使用经过授权且可追溯的协议资料替换合成映射，并逐项验证 CAN ID、字节序、符号、缩放、偏移和物理范围。
- 在目标板上重新验证信号行为，不能以 WSL2 或 CI runner 结果替代板端结果。

SocketCAN 使用与虚拟总线验收指南见 [SOCKETCAN_VIRTUAL_PLAN.md](SOCKETCAN_VIRTUAL_PLAN.md)。该文档不定义真实车辆协议，V2.4 的合成映射也不代表物理 CAN 或 i.MX6ULL 已验证。

## 明确不在当前范围内的内容

- 未定义任何真实车辆 CAN 报文、DBC、诊断协议或真实传感器格式；仓库中的唯一映射是明确标注的合成测试数据。
- 未声称已通过 i.MX6ULL 实机测试。
- 未选择 Yocto、Buildroot、Debian 或厂商 BSP；这些必须由目标项目决定。
- 未提供硬实时保证或功能安全认证。
- WSL2 仅完成 systemd unit 静态验证和临时根脚本测试，未替代目标发行版的服务启动验证。
