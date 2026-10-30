# IMX6ULL Smart Car Terminal V2.6

这是一个面向嵌入式 Linux 学习与实习作品展示的 C++17 智能车载终端项目。默认使用兼容旧版本的有限次数模拟监控；也可选择连续模式，持续运行到收到 SIGINT/SIGTERM。日志和 CSV 支持线程安全的大小轮转与保留上限。可选 SocketCAN 模式通过 Linux 原生经典 CAN socket 接收原始帧，经显式配置的映射转换为车辆数据。

> 当前版本只在 Ubuntu 24.04 / WSL2 的 x86-64 Linux 用户空间中完成适用范围内的本地构建和测试。当前环境没有 `vcan0`，因此真实 SocketCAN 收发集成测试未执行；解析测试使用明确标注的合成字节。项目没有真实车辆协议、DBC 或 i.MX6ULL 实机验证。报警阈值仅为演示参数，不是车辆诊断或安全标准。GitHub Actions 尚未实际运行。

## 功能

- 从无第三方依赖的 `key=value` 配置文件读取阈值、采样参数和输出路径。
- 监测低电压、高温与超速状态，并明确显示 `NORMAL` 或 `WARNING`。
- 线程安全的 INFO、WARNING、ERROR 日志模块，同时输出到控制台和文件。
- CSV 数据记录，包含样本编号、UTC 时间戳、三项数值和状态。
- 日志和 CSV 使用 rename 轮转，CSV 每个新文件重新写入表头，并限制归档数量。
- 有界生产者—消费者队列：采集线程产生样本，处理线程判断状态并写入日志/CSV。
- `finite` 与 `continuous` 两种明确运行模式；连续模式由信号安全停止并排空队列。
- CMake + Ninja 构建，启用 C++17 和 `-Wall -Wextra -Wpedantic`。
- 不依赖测试框架的命令行自动化测试，通过 CTest 执行。
- 提供 `--help`、`--version` 和 `--config <path>` 命令行接口。
- 提供 GNUInstallDirs 安装规则、安装版配置和 i.MX6ULL 移植准备文档。
- 提供可选 systemd unit、连续模拟服务配置和安全的安装/卸载脚本。
- SIGINT/SIGTERM 只在信号处理函数中记录信号，主流程随后停止采集、排空队列并连接线程。
- GitHub Actions 配置 Debug、Release、ASan/UBSan 和原生 Ubuntu ThreadSanitizer 作业。
- Linux SocketCAN 经典 CAN 接收器：RAII 文件描述符、接口绑定、内核过滤、错误帧接收、`poll` 超时和 EINTR 处理。
- 原始 CAN 帧与 `VehicleData` 解析分离；映射显式描述 ID、格式、字节位置、长度、字节序、符号、缩放和偏移。
- 默认模拟模式保持兼容；SocketCAN 配置缺失或接口不可用时明确失败，不静默回退模拟器。

## 工程结构

```text
.
├── .github/workflows/
│   └── ci.yml
├── CMakeLists.txt
├── README.md
├── cmake/toolchains/
│   └── imx6ull-linux-example.cmake
├── config/
│   ├── continuous-simulator-example.conf
│   ├── default.conf
│   ├── default-install.conf.in
│   ├── systemd-simulator.conf.in
│   └── socketcan-synthetic-example.conf
├── docs/
│   ├── IMX6ULL_PORTING_GUIDE.md
│   ├── SYSTEMD_DEPLOYMENT.md
│   └── SOCKETCAN_VIRTUAL_PLAN.md
├── include/car_terminal/
│   ├── blocking_queue.h
│   ├── can_parser.h
│   ├── can_types.h
│   ├── cli.h
│   ├── configuration.h
│   ├── csv_recorder.h
│   ├── file_rotation.h
│   ├── logger.h
│   ├── monitoring_service.h
│   ├── run_mode.h
│   ├── sample_processor.h
│   ├── simulator.h
│   ├── signal_handler.h
│   ├── socket_can_receiver.h
│   ├── socket_can_source.h
│   ├── status_checker.h
│   └── vehicle_data.h
├── src/
│   ├── can_parser.cpp
│   ├── configuration.cpp
│   ├── cli.cpp
│   ├── csv_recorder.cpp
│   ├── file_rotation.cpp
│   ├── logger.cpp
│   ├── monitoring_service.cpp
│   ├── sample_processor.cpp
│   ├── simulator.cpp
│   ├── signal_handler.cpp
│   ├── socket_can_receiver.cpp
│   ├── socket_can_source.cpp
│   └── status_checker.cpp
├── packaging/systemd/
│   ├── imx6ull-car-terminal.service.in
│   └── socketcan-capability.conf
├── scripts/
│   ├── install-systemd.sh
│   └── uninstall-systemd.sh
├── tests/
│   ├── test_configuration.cpp
│   ├── test_blocking_queue.cpp
│   ├── test_can_parser.cpp
│   ├── test_cli.cpp
│   ├── test_io.cpp
│   ├── test_main.cpp
│   ├── test_signal.cpp
│   ├── test_signal_shutdown.sh
│   ├── test_socket_can.cpp
│   ├── test_status.cpp
│   ├── test_systemd_scripts.sh
│   ├── test_systemd_unit.sh
│   ├── test_support.h
│   ├── test_threading.cpp
│   ├── test_vcan_integration.sh
│   └── socketcan_test_sender.cpp
└── main.cpp
```

`build*`、`runtime/`、根目录二进制和本地备份由 `.gitignore` 排除，不属于源代码。

## 模块与数据流

1. `configuration` 解析并验证配置，拒绝缺失、重复、未知、格式错误或越界参数。
2. `simulator` 在采集线程中按采样间隔产生确定性的模拟数据。
3. `BlockingQueue` 使用 `mutex` 和两个 `condition_variable` 实现有界阻塞队列与背压。
4. `MonitoringService` 管理生产者、消费者、停止通知、异常传播和线程连接。
5. `status_checker` 是无共享状态的核心业务逻辑，可脱离线程独立测试。
6. `SampleProcessor` 在消费线程中显示数据，调用日志模块并写入 CSV。
7. `SocketCanReceiver` 仅负责 Linux socket、过滤、超时及原始帧边界；`CanVehicleDataParser` 仅负责配置映射和数值转换。
8. `SocketCanDataSource` 在接收超时后继续等待，并通过通用 `AcquisitionStopped` 与现有监控线程协作退出。
9. `file_rotation` 以同文件系统 rename 移动归档；Logger 和 CsvRecorder 在各自互斥锁内完成检查、轮转和完整记录写入。

```text
VehicleSimulator (producer thread)
        │
        ▼
 bounded BlockingQueue
        │
        ▼
SampleProcessor (consumer thread)
        ├── status_checker
        ├── console + Logger
        └── CsvRecorder
```

SocketCAN 模式只替换生产者侧采集回调：

```text
Linux PF_CAN socket
        │ classic RawCanFrame
        ▼
CanVehicleDataParser ──► VehicleData ──► 原有 BlockingQueue/处理链
```

队列容量可配置。队列满时生产者阻塞，不忙等待；队列空时消费者阻塞。`finite` 模式在处理配置的样本数后结束；`continuous` 模式不设内部样本上限，每次采集后使用可中断的条件变量定时等待。任何线程发生异常都会关闭队列、通知所有等待者，两个线程完成 `join` 后异常才传播到主线程，因此不会遗留可连接线程。`AcquireFunction` 签名保持不变；SocketCAN 另外注册一个只置原子标志的取消回调，使消费者 I/O 失败时也能终止正在进行的“超时—重试”采集循环。

收到 SIGINT 或 SIGTERM 后，异步信号处理函数只写入 `volatile sig_atomic_t`。采集线程在开始下一次采集前观察该标志；若信号在一次采集中到达，该样本仍会入队。随后生产者关闭队列，消费者排空已入队样本并退出，两个线程均由主线程 `join`。采集回调本身仍必须具有有界超时，否则任何同步关闭方案都无法中断永久阻塞的硬件读取。

## 开发环境与依赖

已验证环境：

- Ubuntu 24.04 LTS（WSL2）
- GCC/G++ 13.3
- CMake 3.28
- Ninja 1.11
- CTest（随 CMake 提供）
- POSIX Threads（由 CMake `Threads::Threads` 配置）

项目只使用 C++ 标准库和系统线程库，不需要安装额外第三方依赖，也不依赖 VS Code。

## 构建和测试

在项目根目录执行：

```bash
cmake -S . -B build_v25 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build_v25 --verbose
ctest --test-dir build_v25 --output-on-failure
```

运行：

```bash
./build_v25/car_terminal --config config/default.conf
```

不传参数时默认读取当前工作目录下的 `config/default.conf`：

```bash
./build_v25/car_terminal
```

也保留单个位置参数形式以兼容 V2.0：

```bash
./build_v25/car_terminal config/default.conf
```

命令行帮助和版本：

```bash
./build_v25/car_terminal --help
./build_v25/car_terminal --version
```

连续模拟监控会一直运行到收到 Ctrl+C（SIGINT）或 SIGTERM：

```bash
./build_v25/car_terminal \
  --config config/continuous-simulator-example.conf
```

未知选项、缺少 `--config` 路径或多余参数会显示用法并以状态码 2 退出。配置错误同样以状态码 2 退出；运行期文件或处理错误以状态码 1 退出。`log_file` 和 `csv_file` 不允许指向同一文件，也不允许覆盖当前配置文件，以避免日志追加与 CSV 截断破坏配置或彼此数据。

SIGINT 按约定以 130 退出，SIGTERM 以 143 退出。退出前会完成已入队样本处理、刷新日志和 CSV，并输出文件路径。

## 安装

建议在配置阶段明确安装前缀。下例使用独立目录，不会写入系统路径：

```bash
cmake -S . -B build_install -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/install-root" \
  -DBUILD_TESTING=OFF
cmake --build build_install
cmake --install build_install
```

安装内容：

```text
install-root/bin/car_terminal
install-root/etc/imx6ull-car-terminal/continuous-simulator-example.conf
install-root/etc/imx6ull-car-terminal/default.conf
install-root/etc/imx6ull-car-terminal/systemd.conf
install-root/etc/imx6ull-car-terminal/socketcan-synthetic-example.conf
install-root/lib/systemd/system/imx6ull-car-terminal.service
install-root/share/imx6ull-car-terminal/scripts/install-systemd.sh
install-root/share/imx6ull-car-terminal/scripts/uninstall-systemd.sh
install-root/share/imx6ull-car-terminal/systemd/socketcan-capability.conf
install-root/share/doc/imx6ull-car-terminal/README.md
install-root/share/doc/imx6ull-car-terminal/IMX6ULL_PORTING_GUIDE.md
install-root/share/doc/imx6ull-car-terminal/SYSTEMD_DEPLOYMENT.md
install-root/share/doc/imx6ull-car-terminal/SOCKETCAN_VIRTUAL_PLAN.md
```

安装版程序应显式指定安装版配置，因此可从任意工作目录启动：

```bash
"$PWD/install-root/bin/car_terminal" \
  --config "$PWD/install-root/etc/imx6ull-car-terminal/default.conf"
```

首次运行会在同一安装前缀的 `var/log/imx6ull-car-terminal/` 和 `var/lib/imx6ull-car-terminal/` 下创建日志和 CSV。系统级安装时必须为运行用户配置这些目录的写权限。

### systemd（可选）

systemd 部署使用独立的连续模拟配置，不改变 `default.conf` 的 finite 默认行为。先完成 CMake 安装，再运行安装脚本；脚本默认不启动服务，也不授予 capability：

```bash
sudo /usr/local/share/imx6ull-car-terminal/scripts/install-systemd.sh --enable
sudo systemctl start imx6ull-car-terminal.service
sudo systemctl status imx6ull-car-terminal.service
```

服务以 `car-terminal` 用户运行，使用 `StateDirectory`/`LogsDirectory` 管理可写目录，异常退出采用 `Restart=on-failure`，停止使用 SIGTERM 和现有队列排空路径。SocketCAN 可按需用 `--socketcan` 安装只包含 `CAP_NET_RAW` 的 drop-in；模拟模式不获得该权限。完整操作和卸载行为见 [docs/SYSTEMD_DEPLOYMENT.md](docs/SYSTEMD_DEPLOYMENT.md)。当前 WSL2 只完成静态 unit 和临时安装根测试，没有实际启动 systemd 服务。

## 配置文件

默认配置位于 `config/default.conf`。

| 参数 | 含义 | 默认值 |
|---|---|---:|
| `low_voltage_threshold_v` | 低电压演示阈值 | 12.55 |
| `high_temperature_threshold_c` | 高温演示阈值 | 40.0 |
| `overspeed_threshold_kmh` | 超速演示阈值 | 120.0 |
| `run_mode` | `finite` 或 `continuous`；省略时兼容为 `finite` | `finite` |
| `sample_count` | `finite` 的总采样次数；连续模式仍解析但不作为停止条件 | 10 |
| `sampling_interval_ms` | 采样间隔（毫秒） | 100 |
| `queue_capacity` | 生产者—消费者队列容量 | 4 |
| `log_max_size_bytes` | 活动日志文件软上限（字节） | 10485760 |
| `csv_max_size_bytes` | 活动 CSV 文件软上限（字节） | 52428800 |
| `rotated_file_count` | 每种输出保留的轮转文件数，范围 1–100 | 5 |
| `log_file` | 日志路径 | `../runtime/car_terminal.log` |
| `csv_file` | CSV 路径 | `../runtime/vehicle_data.csv` |
| `data_source` | `simulator` 或 `socketcan` | `simulator` |

相对输出路径相对于配置文件所在目录解析，而不是相对于可执行文件。配置解析器会拒绝未知键、重复键、缺失键、非数字值和不合理范围。旧配置没有 `run_mode` 时保持有限次数运行；旧配置没有三个轮转键时自动使用表中默认值。

## SocketCAN 配置与运行

只有 `data_source=socketcan` 时才允许并要求以下键：

| 参数 | 含义 |
|---|---|
| `can_interface` | Linux CAN 网络接口名称，例如 `can0` 或已存在的 `vcan0` |
| `can_receive_timeout_ms` | 每次 `poll` 的最大等待时间，范围 1–60000 ms |
| `can_filters` | 逗号分隔的 `format:id:mask` 内核过滤规则 |
| `can_speed_mapping` | 车速映射 |
| `can_temperature_mapping` | 温度映射 |
| `can_voltage_mapping` | 电压映射 |

过滤格式中的 `format` 是 `standard` 或 `extended`。标准 ID 最大为 `0x7FF`，扩展 ID 最大为 `0x1FFFFFFF`；掩码必须非零且适合对应格式。映射 ID 必须能被至少一条过滤规则接收。

每条信号映射使用：

```text
format:id:byte_offset:byte_length:byte_order:value_type:scale:value_offset
```

- `byte_order`：`little` 或 `big`；
- `value_type`：`unsigned` 或二进制补码 `signed`；
- 字节范围必须落在经典 CAN 的 8 字节负载内；实际帧 DLC 仍必须覆盖映射范围；
- 缩放和偏移必须为有限数，缩放不能为零；计算结果必须能表示为有限 `double`。

[config/socketcan-synthetic-example.conf](config/socketcan-synthetic-example.conf) 仅定义用于软件/vcan 测试的合成 `0x123` 帧，不代表任何真实车辆协议。运行方式：

```bash
./build_v26/car_terminal \
  --config config/socketcan-synthetic-example.conf
```

接口不存在、映射不完整、过滤器非法或 socket/bind 失败时，程序输出明确错误并非零退出，不会回退模拟模式。读取使用有限 `poll` 超时；超时不生成虚假样本，收到 SIGINT/SIGTERM 后最迟在当前超时结束时进入正常排空流程。经典远程帧和错误帧会被明确拒绝为车辆数据；CAN FD 未启用且不在 V2.4 范围内。

若系统已经存在可用的 `vcan0`，可运行：

```bash
ctest --test-dir build_v26 -R vcan --output-on-failure -V
```

测试会发送合成帧并验证过滤、超时等待、解析和 CSV；若 `vcan0` 不存在则返回 CTest skip code 77，必须记录为“未执行”。项目不会自动创建接口、加载模块或安装 can-utils。

边界规则经过自动化测试：

- 电压严格低于阈值时报警；等于阈值不报警。
- 温度大于或等于阈值时报警。
- 速度严格高于阈值时报警；等于阈值不报警。

## 日志与 CSV

默认运行生成：

- `runtime/car_terminal.log`：追加写入的 INFO/WARNING/ERROR 日志。
- `runtime/vehicle_data.csv`：每次运行重新创建，包含表头和本次全部样本。

活动文件在“当前大小 + 下一条完整记录”超过阈值前轮转：原文件原子 rename 为 `.1`，已有归档依次移动为 `.2`、`.3`，超过 `rotated_file_count` 的最旧文件被删除。CSV 新活动文件会先写标准表头。单条记录本身大于阈值时允许该活动文件暂时超过阈值，下一条记录前再轮转，因此阈值是软上限而不会拆分日志行或 CSV 行。这里没有使用 copytruncate。

检查、rename、重建和完整记录写入都位于模块互斥锁内。目录创建、文件检查、删除、重命名、打开、刷新或写入失败会抛出带路径的明确错误并停止运行，不会静默忽略失败。

## 自动化测试范围

- 正常状态、低电压、高温、超速和三种报警组合。
- 三个阈值恰好相等时的边界规则。
- 合法配置、文件缺失、键缺失、格式错误、未知键、重复键、无效数字和越界值。
- INFO/WARNING/ERROR 日志内容与 CSV 表头、数据、组合状态。
- 日志/CSV 文件打开失败，以及对象析构后文件可再次使用。
- 有界队列的顺序、全部样本处理、消费者异常后的停止、异常传播和线程连接。
- finite 次数兼容、continuous 持续采样、信号停止和连续模式异常退出。
- 满队列生产者和空队列消费者在关闭时被唤醒，以及关闭后队列数据的排空。
- 并发日志/CSV 完整性、真实写失败、生产者异常和命令行解析。
- 日志/CSV 多次轮转、保留数量、CSV 新表头、轮转失败和并发轮转数据完整性。
- systemd unit 指令、`systemd-analyze verify` 以及临时根目录安装/卸载行为。
- 正常监测完成、SIGINT/SIGTERM 标志记录、信号停止后的已采集样本排空。
- 进程级 SIGINT/SIGTERM 测试：连续模式越过有限样本数、退出码、队列排空、停止日志及 CSV/日志样本数一致性。
- 标准/扩展 CAN ID、过滤器、DLC、大小端、有符号转换、缩放/偏移、溢出和多帧完整性。
- SocketCAN 超时重试、停止协作、接收错误传播和不存在接口的初始化错误。
- 可选 vcan 集成测试；只有环境中已有 `vcan0` 时才实际发送帧。

## GitHub Actions CI

`.github/workflows/ci.yml` 在 Ubuntu 24.04 runner 上配置以下作业：

- Debug 和 Release 的 CMake、Ninja 构建与完整 CTest；
- ASan/UBSan 构建与 CTest；
- 独立的 ThreadSanitizer 构建与 CTest；
- 独立 SocketCAN vcan 作业：在 runner 内准备 `vcan0`，运行集成测试，并把任何 skip 视为失败。

TSan 和 vcan 作业都没有设置 `continue-on-error`，检测或环境准备失败会保留日志并使作业失败。当前仓库尚未推送和触发该工作流，因此只能确认配置文件已完成本地语法与内容检查，不能宣称 GitHub CI、原生 Ubuntu TSan 或 CI vcan 已通过。

## 已知限制

- 默认采集仍为模拟器；可选 SocketCAN 接收和可配置解析已经实现，但当前没有真实 CAN/vcan 收发验证，也没有任何已证实车辆映射。
- 没有 UART、I2C、SPI、GPIO 或真实传感器接口。
- 没有在 ARM 或 i.MX6ULL 开发板上交叉编译、部署或实机验证。
- `std::thread` 调度不是硬实时，采样间隔不能作为实时保证。
- CSV 每次进程启动仍重新创建；大小轮转不等同于掉电保护或事务存储。
- 轮转限制单个文件和归档数量，但磁盘写满、底层文件系统损坏及归档被外部程序占用仍会使进程明确失败。
- systemd 支持是可选部署模板；当前 WSL2 未实际启动服务，非 systemd 发行版需要使用自己的 init 管理方式。
- CSV 使用系统墙钟记录 UTC 时间；系统校时可能让相邻时间戳不单调，样本顺序应以 `sample` 字段为准。
- 模拟器数值固定递增/递减，不代表任何真实车辆模型。
- 信号停止无法抢占永久阻塞的采集回调；未来硬件数据源必须提供读取超时。
- 只支持经典 CAN 0–8 字节数据帧；CAN FD 和远程帧数据解析不在范围内，错误帧被报告而非转换为车辆数据。

## 后续 i.MX6ULL 移植计划

详细前置条件、工具链模板、部署流程和板端验收清单见 [docs/IMX6ULL_PORTING_GUIDE.md](docs/IMX6ULL_PORTING_GUIDE.md)。

1. 为 ARM 工具链编写 CMake toolchain 文件，先完成交叉编译和目标根文件系统依赖检查。
2. 复用 `MonitoringService::AcquireFunction` 采集注入边界，把硬件适配器绑定为采集回调；只有出现多个复杂数据源时再引入虚基类接口。
3. 按实际硬件方案接入 SocketCAN、UART、I2C/SPI 或 sysfs/libgpiod，并为设备 I/O 设置超时和重连。
4. 在开发板上验证线程、文件系统、时钟、异常退出和长时间运行，不把 x86 测试结果当作实机结果。
5. 在目标发行版上验证 systemd 或其他 init 集成、只读根文件系统适配和交叉平台 CI。

SocketCAN 使用方法、合成帧格式、vcan 前置条件和验收边界见 [docs/SOCKETCAN_VIRTUAL_PLAN.md](docs/SOCKETCAN_VIRTUAL_PLAN.md)。当前环境没有 `vcan0`，所以只完成了构建、纯解析/替身单元测试和不存在接口错误路径验证，未完成真实 socket 收发集成验证。

## V2.6 相对于 V2.5

- 新增日志和 CSV 大小轮转、归档保留上限、CSV 表头重建及明确 I/O 错误传播。
- 轮转与写入共享模块内互斥锁，不使用 copytruncate，并增加并发完整性测试。
- 新增非 root systemd unit、连续模拟服务配置、异常重启策略和安全停止设置。
- SocketCAN capability 通过可选 drop-in 单独授予，默认模拟服务不获得网络能力。
- 新增临时根安装/卸载测试和 systemd 静态验证；未声称完成目标板服务启动。

## V2.5 相对于 V2.4

- 新增显式 `finite`/`continuous` 运行模式，旧配置默认保持 finite。
- 连续模式复用有界阻塞队列、采样间隔等待、异常传播和线程连接机制。
- SIGINT/SIGTERM 后停止生产、排空队列、刷新日志与 CSV，并分别返回 130/143。
- 新增连续模拟配置、配置校验、线程异常、SocketCAN 超时停止及双信号进程级测试。
- 版本号更新为 2.5.0；没有声称完成真实 CAN 或 i.MX6ULL 实机验证。

## V2.4 相对于 V2.3

- 新增 Linux SocketCAN 经典 CAN 接收器、RAII 资源管理、过滤、超时和系统错误处理。
- 新增独立原始帧模型、可配置三信号映射解析器及数据源适配器。
- 默认模拟模式保持兼容；增加明确的 SocketCAN 示例配置与错误路径。
- 新增 27 项 CAN、配置与采集取消相关单元行为覆盖及可选 vcan 进程级集成测试。
- 保留 V2.3 的信号安全退出、队列排空、CI 和所有原有测试。
