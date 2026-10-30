# SocketCAN 使用与虚拟总线验收指南

## 当前实现边界

V2.4 使用 Linux 原生 `PF_CAN` / `SOCK_RAW` / `CAN_RAW` 接收经典 CAN 帧，并通过现有 `MonitoringService::AcquireFunction` 注入监控流程。接收层只处理 socket、接口绑定、内核过滤、有限超时和原始帧边界；解析层依据显式配置把原始帧转换为 `VehicleData`。默认运行模式仍是确定性模拟器。

仓库没有真实车辆 DBC 或协议。`config/socketcan-synthetic-example.conf` 中的 `0x123` 帧和三个信号全部是软件测试用的合成数据，不代表任何车型、ECU 或诊断标准。缺少映射或接口不可用时程序明确失败，不会静默回退模拟器。

当前 WSL2 环境有 Linux CAN 头文件、`ip`、`modprobe` 和 `modinfo`，但没有已存在的 `vcan0`，也没有 `candump`/`cansend`。根据本项目的保护要求，没有安装工具、加载模块、创建接口或使用 `sudo`。因此本地 vcan 集成测试被 CTest 明确标记为跳过，不能记录为通过。

## 配置格式

选择 SocketCAN 数据源：

```ini
data_source=socketcan
can_interface=vcan0
can_receive_timeout_ms=100
can_filters=standard:0x123:0x7FF
can_speed_mapping=standard:0x123:0:2:little:unsigned:0.01:0.0
can_temperature_mapping=standard:0x123:2:2:little:signed:0.1:0.0
can_voltage_mapping=standard:0x123:4:2:big:unsigned:0.01:0.0
```

过滤器格式为：

```text
帧格式:CAN_ID:掩码
```

映射格式为：

```text
帧格式:CAN_ID:字节偏移:字节长度:字节序:符号类型:缩放系数:数值偏移
```

帧格式只能是 `standard` 或 `extended`；字节序只能是 `little` 或 `big`；符号类型只能是 `unsigned` 或 `signed`。解析器检查 CAN ID、掩码、映射重复、字节范围、数据长度、有限数值和转换溢出。三个信号都收到新值后才生成一个 `VehicleData` 样本，不会用猜测值补齐缺失信号。

## 程序行为

- 使用有限 `poll` 超时，不会永久阻塞采集线程。
- 超时代表“当前无数据”，不会生成样本；停止谓词会在等待边界被检查。
- `EINTR` 会重新计算剩余等待时间后重试。
- 标准帧与扩展帧的过滤语义保持分离。
- 经典 CAN 数据长度必须在 0–8 字节内；解析还要求满足对应映射长度。
- RTR 和错误帧会被明确报告，不能转换为车辆样本。
- CAN FD 未启用，也不在 V2.4 范围内。
- socket 文件描述符由 RAII 对象管理；构造或接收错误路径也会关闭资源。
- SIGINT/SIGTERM 处理函数只设置 `sig_atomic_t` 标志；主线程/采集循环负责关闭队列、排空已入队样本、连接线程并刷新日志与 CSV。

## vcan 集成测试

测试目标 `car_terminal_vcan_integration` 只使用已经存在且已启用的 `vcan0`。它不会自行使用 `sudo`、加载模块或修改网络配置。没有 `vcan0` 时返回 CTest 跳过码 77。

在获得系统管理员授权、且原生 Linux 环境已具备 vcan 支持时，可由操作者在项目外准备接口：

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
ip -details link show vcan0
```

这些命令只建立 Linux 虚拟 CAN 网络接口，不表示已经连接真实车辆。项目的集成测试自带一个最小 C++ 发送器，不依赖 `cansend`：

```bash
cmake -S . -B build-vcan -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-vcan
ctest --test-dir build-vcan -R car_terminal_vcan_integration -V
```

测试会先发送应被过滤掉的合成 `0x124` 帧，再发送两个 `0x123` 合成帧，验证内核过滤、无数据等待、映射解析、CSV 完整性和正常退出。只有 CTest 显示 Passed 才能记录为 vcan 集成通过；Skipped 不能算通过。

`.github/workflows/ci.yml` 还配置了独立的原生 Ubuntu vcan 作业：它在 runner 内安装构建工具、`iproute2` 和 `kmod`，创建 `vcan0`，并将集成测试的任何 skip 视为失败。该工作流尚未被远程触发，配置存在不等于 CI 已通过；若 runner 不允许加载模块或创建网络接口，作业应如实失败并保留诊断。

## 目标板前置条件

实际 i.MX6ULL 接入还需要从所选板卡/BSP 文档确认：

- CAN 控制器驱动与 Linux CAN 网络设备是否可用；
- 设备树、管脚复用、时钟、收发器和电气终端；
- 接口位时序/波特率配置责任和启动服务；
- 运行用户访问网络设备和日志目录的权限；
- 与目标 ABI、libstdc++ 和内核 UAPI 匹配的工具链/sysroot；
- 经过授权且可追溯的真实协议资料及物理量范围。

本工程不会猜测 CAN 引脚、BSP 参数、波特率或车辆帧格式。建议下一阶段先在具备 `vcan0` 的原生 Linux CI/主机上完成集成测试，再使用已知的实验总线和合成帧完成物理收发器冒烟测试，最后才接入经授权的协议解析。
