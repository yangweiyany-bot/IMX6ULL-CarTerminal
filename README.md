# IMX6ULL Smart Car Terminal V2.2

这是一个面向嵌入式 Linux 学习与实习作品展示的 C++17 智能车载终端模拟项目。程序在两个线程中模拟车辆数据采集和监测处理，检测车速、温度与电压状态，并把运行日志及采样数据分别保存为日志文件和 CSV。

> 当前版本只在 Ubuntu 24.04 / WSL2 的 x86-64 Linux 用户空间中完成构建和测试。数据来自确定性的模拟器，不是传感器、CAN 总线或真实车辆数据；项目尚未经过 i.MX6ULL 实机验证。所有报警阈值仅为演示参数，不是车辆诊断或安全标准。

## 功能

- 从无第三方依赖的 `key=value` 配置文件读取阈值、采样参数和输出路径。
- 监测低电压、高温与超速状态，并明确显示 `NORMAL` 或 `WARNING`。
- 线程安全的 INFO、WARNING、ERROR 日志模块，同时输出到控制台和文件。
- CSV 数据记录，包含样本编号、UTC 时间戳、三项数值和状态。
- 有界生产者—消费者队列：采集线程产生样本，处理线程判断状态并写入日志/CSV。
- CMake + Ninja 构建，启用 C++17 和 `-Wall -Wextra -Wpedantic`。
- 不依赖测试框架的命令行自动化测试，通过 CTest 执行。
- 提供 `--help`、`--version` 和 `--config <path>` 命令行接口。
- 提供 GNUInstallDirs 安装规则、安装版配置和 i.MX6ULL 移植准备文档。

## 工程结构

```text
.
├── CMakeLists.txt
├── README.md
├── cmake/toolchains/
│   └── imx6ull-linux-example.cmake
├── config/
│   ├── default.conf
│   └── default-install.conf.in
├── docs/
│   └── IMX6ULL_PORTING_GUIDE.md
├── include/car_terminal/
│   ├── blocking_queue.h
│   ├── cli.h
│   ├── configuration.h
│   ├── csv_recorder.h
│   ├── logger.h
│   ├── monitoring_service.h
│   ├── sample_processor.h
│   ├── simulator.h
│   ├── status_checker.h
│   └── vehicle_data.h
├── src/
│   ├── configuration.cpp
│   ├── cli.cpp
│   ├── csv_recorder.cpp
│   ├── logger.cpp
│   ├── monitoring_service.cpp
│   ├── sample_processor.cpp
│   ├── simulator.cpp
│   └── status_checker.cpp
├── tests/
│   ├── test_configuration.cpp
│   ├── test_blocking_queue.cpp
│   ├── test_cli.cpp
│   ├── test_io.cpp
│   ├── test_main.cpp
│   ├── test_status.cpp
│   ├── test_support.h
│   └── test_threading.cpp
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

队列容量可配置。队列满时生产者阻塞，不忙等待；队列空时消费者阻塞。任何线程发生异常都会关闭队列、通知所有等待者，两个线程完成 `join` 后异常才传播到主线程，因此不会遗留可连接线程。

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
cmake -S . -B build_v22 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build_v22 --verbose
ctest --test-dir build_v22 --output-on-failure
```

运行：

```bash
./build_v22/car_terminal --config config/default.conf
```

不传参数时默认读取当前工作目录下的 `config/default.conf`：

```bash
./build_v22/car_terminal
```

也保留单个位置参数形式以兼容 V2.0：

```bash
./build_v22/car_terminal config/default.conf
```

命令行帮助和版本：

```bash
./build_v22/car_terminal --help
./build_v22/car_terminal --version
```

未知选项、缺少 `--config` 路径或多余参数会显示用法并以状态码 2 退出。配置错误同样以状态码 2 退出；运行期文件或处理错误以状态码 1 退出。`log_file` 和 `csv_file` 不允许指向同一文件，也不允许覆盖当前配置文件，以避免日志追加与 CSV 截断破坏配置或彼此数据。

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
install-root/etc/imx6ull-car-terminal/default.conf
install-root/share/doc/imx6ull-car-terminal/README.md
install-root/share/doc/imx6ull-car-terminal/IMX6ULL_PORTING_GUIDE.md
```

安装版程序应显式指定安装版配置，因此可从任意工作目录启动：

```bash
"$PWD/install-root/bin/car_terminal" \
  --config "$PWD/install-root/etc/imx6ull-car-terminal/default.conf"
```

首次运行会在同一安装前缀的 `var/log/imx6ull-car-terminal/` 和 `var/lib/imx6ull-car-terminal/` 下创建日志和 CSV。系统级安装时必须为运行用户配置这些目录的写权限。

## 配置文件

默认配置位于 `config/default.conf`。

| 参数 | 含义 | 默认值 |
|---|---|---:|
| `low_voltage_threshold_v` | 低电压演示阈值 | 12.55 |
| `high_temperature_threshold_c` | 高温演示阈值 | 40.0 |
| `overspeed_threshold_kmh` | 超速演示阈值 | 120.0 |
| `sample_count` | 总采样次数 | 10 |
| `sampling_interval_ms` | 采样间隔（毫秒） | 100 |
| `queue_capacity` | 生产者—消费者队列容量 | 4 |
| `log_file` | 日志路径 | `../runtime/car_terminal.log` |
| `csv_file` | CSV 路径 | `../runtime/vehicle_data.csv` |

相对输出路径相对于配置文件所在目录解析，而不是相对于可执行文件。配置解析器会拒绝未知键、重复键、缺失键、非数字值和不合理范围。

边界规则经过自动化测试：

- 电压严格低于阈值时报警；等于阈值不报警。
- 温度大于或等于阈值时报警。
- 速度严格高于阈值时报警；等于阈值不报警。

## 日志与 CSV

默认运行生成：

- `runtime/car_terminal.log`：追加写入的 INFO/WARNING/ERROR 日志。
- `runtime/vehicle_data.csv`：每次运行重新创建，包含表头和本次全部样本。

模块会创建缺失的父目录。目录创建、文件打开或文件写入失败会抛出带路径的明确错误；程序会记录能够记录的 ERROR 信息并返回非零状态码。

## 自动化测试范围

- 正常状态、低电压、高温、超速和三种报警组合。
- 三个阈值恰好相等时的边界规则。
- 合法配置、文件缺失、键缺失、格式错误、未知键、重复键、无效数字和越界值。
- INFO/WARNING/ERROR 日志内容与 CSV 表头、数据、组合状态。
- 日志/CSV 文件打开失败，以及对象析构后文件可再次使用。
- 有界队列的顺序、全部样本处理、消费者异常后的停止、异常传播和线程连接。
- 满队列生产者和空队列消费者在关闭时被唤醒，以及关闭后队列数据的排空。
- 并发日志/CSV 完整性、真实写失败、生产者异常和命令行解析。

## 已知限制

- 当前仅有模拟采集，没有 CAN、UART、I2C、SPI、GPIO 或真实传感器接口。
- 没有在 ARM 或 i.MX6ULL 开发板上交叉编译、部署或实机验证。
- `std::thread` 调度不是硬实时，采样间隔不能作为实时保证。
- CSV 每次运行覆盖，日志则追加；生产系统通常还需要轮转、存储上限和掉电保护。
- CSV 使用系统墙钟记录 UTC 时间；系统校时可能让相邻时间戳不单调，样本顺序应以 `sample` 字段为准。
- 模拟器数值固定递增/递减，不代表任何真实车辆模型。

## 后续 i.MX6ULL 移植计划

详细前置条件、工具链模板、部署流程和板端验收清单见 [docs/IMX6ULL_PORTING_GUIDE.md](docs/IMX6ULL_PORTING_GUIDE.md)。

1. 为 ARM 工具链编写 CMake toolchain 文件，先完成交叉编译和目标根文件系统依赖检查。
2. 复用 `MonitoringService::AcquireFunction` 采集注入边界，把硬件适配器绑定为采集回调；只有出现多个复杂数据源时再引入虚基类接口。
3. 按实际硬件方案接入 SocketCAN、UART、I2C/SPI 或 sysfs/libgpiod，并为设备 I/O 设置超时和重连。
4. 在开发板上验证线程、文件系统、时钟、异常退出和长时间运行，不把 x86 测试结果当作实机结果。
5. 根据部署需求增加 systemd 服务、日志轮转、只读根文件系统适配和交叉平台 CI。
