# systemd 部署指南

本指南适用于确实使用 systemd 的 Linux 发行版。Yocto、Buildroot、厂商 BSP
或精简根文件系统可能使用其他 init 系统；此时保留相同的非 root 用户、目录权限、
SIGTERM 和重启策略即可，不要强行引入 systemd。

## 安装

先构建并安装到目标系统。以下示例使用默认 `/usr/local` 前缀：

```bash
cmake -S . -B build_release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build_release
sudo cmake --install build_release
sudo /usr/local/share/imx6ull-car-terminal/scripts/install-systemd.sh --enable
```

安装脚本不会启动服务。它创建 `car-terminal` 系统用户、安装 activation unit，
并以 `0750` 创建 `/var/log/imx6ull-car-terminal` 和
`/var/lib/imx6ull-car-terminal`。配置保存在
`/usr/local/etc/imx6ull-car-terminal/systemd.conf`，与运行数据分离。

默认 unit 使用连续模拟配置，不授予网络 capability。启动前应先检查配置：

```bash
sudo systemctl daemon-reload
sudo systemctl start imx6ull-car-terminal.service
sudo systemctl status imx6ull-car-terminal.service
sudo systemctl restart imx6ull-car-terminal.service
sudo systemctl stop imx6ull-car-terminal.service
journalctl -u imx6ull-car-terminal.service
```

`systemctl stop` 发送 SIGTERM。程序停止生产、排空队列、刷新日志和 CSV，unit
等待最多 30 秒。`Restart=on-failure` 只负责异常退出后的恢复，不替代应用级错误
诊断。

## SocketCAN 权限

切换 `data_source=socketcan` 前，必须根据实际 BSP 配置 CAN 控制器、收发器、位
时序和网络接口。项目不会创建 `can0`、加载内核模块或猜测车辆协议。

只有目标内核确实要求原始网络 socket capability 时，才安装最小 drop-in：

```bash
sudo /usr/local/share/imx6ull-car-terminal/scripts/install-systemd.sh \
  --socketcan
sudo systemctl daemon-reload
sudo systemctl restart imx6ull-car-terminal.service
```

该 drop-in 只授予 `CAP_NET_RAW`，不会授予 root、`CAP_NET_ADMIN` 或其他能力。
CAN 接口的创建和配置应由 BSP 的网络管理机制负责。

## 卸载服务管理

```bash
sudo /usr/local/share/imx6ull-car-terminal/scripts/uninstall-systemd.sh
```

卸载脚本停止并禁用服务，删除 activation unit 和可选 capability drop-in，但故意
保留可执行文件、配置、服务用户、日志、CSV 和轮转文件，避免意外丢失数据。软件
包本身应由所用镜像或包管理系统卸载。

## 当前验证边界

WSL2 环境只执行了 unit 指令检查、`systemd-analyze verify`、临时根目录中的安装/
卸载脚本测试和安装树检查。没有把本机注册为服务，也没有声称完成目标板 systemd
启动验证。
