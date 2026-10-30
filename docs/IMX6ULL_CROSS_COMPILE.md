# i.MX6ULL ARM 交叉编译指南

本指南建立可复现的 ARM 交叉构建入口，但不替具体开发板选择 ABI、libc、BSP 或
Linux 发行版。i.MX6ULL 是 32 位 ARM Cortex-A7；实际工具链、浮点 ABI、FPU 参数、
动态加载器和 sysroot 必须与开发板正在运行的 Linux 镜像完全匹配。

## 1. 获取匹配的工具链和 sysroot

优先级建议如下：

1. 板卡厂商随 BSP 发布的 SDK 或交叉工具链及 sysroot。
2. 生成目标镜像的 Yocto SDK（`populate_sdk` 产物）。
3. 生成目标镜像的 Buildroot external toolchain/SDK。
4. Debian/Ubuntu 通用 ARM GNU 工具链；只有确认目标 ABI、glibc 版本和动态加载器
   与板端匹配时才可使用。

不要只因为编译器名称包含 `gnueabihf` 就假定板端一定使用 hard-float，也不要把
WSL 的 `/usr` 或 `/` 当作目标 sysroot。sysroot 应至少包含目标头文件、libc、
libstdc++、libgcc、pthread、动态加载器以及与目标镜像一致的库版本。

当前项目不会下载 SDK，也不会自动安装系统软件。

## 2. 通用 GNU/BSP 工具链

仓库提供：

```text
cmake/toolchains/imx6ull-linux-gnu.cmake
scripts/build-imx6ull.sh
scripts/check-arm-elf.sh
```

一键 Release 构建示例：

```bash
./scripts/build-imx6ull.sh \
  --build-type Release \
  --cc /opt/vendor-sdk/bin/<target>-gcc \
  --cxx /opt/vendor-sdk/bin/<target>-g++ \
  --sysroot /opt/vendor-sdk/sysroots/<target> \
  --arch-flags '<flags supplied by the BSP>'
```

Debug 使用 `--build-type Debug`。`--arch-flags` 可以省略；只有 BSP/SDK 明确要求时
才传入 CPU、FPU 和浮点 ABI 参数。脚本会调用编译器的 `-dumpmachine`，拒绝
x86-64 或 AArch64 编译器，并拒绝主机根目录作为 sysroot。

也可以直接使用 CMake：

```bash
cmake -S . -B build_imx6ull_release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/imx6ull-linux-gnu.cmake \
  -DIMX6ULL_C_COMPILER=/opt/vendor-sdk/bin/<target>-gcc \
  -DIMX6ULL_CXX_COMPILER=/opt/vendor-sdk/bin/<target>-g++ \
  -DIMX6ULL_SYSROOT=/opt/vendor-sdk/sysroots/<target> \
  -DIMX6ULL_ARCH_FLAGS='<flags supplied by the BSP>' \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=OFF
```

工具链文件设置 `CMAKE_SYSTEM_NAME=Linux`、`CMAKE_SYSTEM_PROCESSOR=arm`、
`CMAKE_SYSROOT` 和 `CMAKE_FIND_ROOT_PATH`。程序在主机查找；库、头文件和 CMake
package 只允许从目标 sysroot 查找，防止误链接 x86-64 主机库。

## 3. Yocto SDK

Yocto SDK 通常提供 `environment-setup-*`、`SDKTARGETSYSROOT`、`CC`、`CXX` 和
`OECORE_NATIVE_SYSROOT`。使用 SDK 自带的环境脚本和 CMake toolchain：

```bash
./scripts/build-imx6ull.sh \
  --yocto-env /opt/poky/<version>/environment-setup-<target> \
  --build-type Release
```

脚本优先使用环境中的 `CMAKE_TOOLCHAIN_FILE`；若未设置，则检查
`$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake`。如果 SDK 使用
其他位置，可额外传入 `--toolchain-file <path>`。脚本仍会验证 `CC`/`CXX` 的
target 和 `SDKTARGETSYSROOT`，但不会覆盖 SDK 已定义的 ABI 参数。

## 4. 输出目录和安装包

脚本不会使用或覆盖主机端 `build_v26_*`/`build_v27_*` 目录：

```text
build_imx6ull_debug/
build_imx6ull_release/
install_imx6ull_debug/
install_imx6ull_release/
```

构建时使用 `CMAKE_INSTALL_PREFIX=/usr` 和 `DESTDIR`。Release 安装树包含：

```text
install_imx6ull_release/usr/bin/car_terminal
install_imx6ull_release/etc/imx6ull-car-terminal/
install_imx6ull_release/usr/lib/systemd/system/
install_imx6ull_release/usr/share/imx6ull-car-terminal/scripts/
install_imx6ull_release/usr/share/doc/imx6ull-car-terminal/
```

## 5. ARM ELF 与依赖检查

成功构建后脚本自动调用 `check-arm-elf.sh`，并把报告写入安装目录的
`arm-elf-report.txt`。检查内容包括：

- `file` 必须报告 32 位 ARM ELF，且不能包含 x86-64；
- `readelf -h` 的 Class 必须为 ELF32，Machine 必须为 ARM；
- `objdump -f` 必须报告 ARM architecture；
- 动态加载器必须存在于目标 sysroot；
- 每个 `DT_NEEDED` 动态库都必须能在目标 sysroot 中找到；
- 解释器和依赖的 realpath 不得通过绝对符号链接逃出 sysroot；
- sysroot 中匹配到的依赖必须是 32 位 ARM ELF，不得是 x86-64/AArch64；
- `readelf -A` 报告 CPU architecture 与浮点参数，供 BSP ABI 核对。

这只能证明二进制与所给工具链/sysroot 自洽，不能替代板端运行验证。

## 6. 部署和板端验证

把安装树按其绝对布局纳入目标镜像或复制到开发板。不要把主机构建目录、x86-64
二进制或 WSL 系统库复制到板端。部署后至少检查：

```bash
file /usr/bin/car_terminal
readelf -h /usr/bin/car_terminal
readelf -l /usr/bin/car_terminal
/usr/bin/car_terminal --version
/usr/bin/car_terminal --config /etc/imx6ull-car-terminal/default.conf
```

如果板端使用动态链接，还应确认加载器和所有依赖实际存在。先验证 finite 模拟模式，
再验证 continuous、SIGTERM、轮转和 systemd；最后才接入经过授权的真实硬件协议。

## 7. 当前验证边界

当前 WSL2 环境没有 ARM Linux 交叉编译器、Yocto SDK、ARM sysroot 或 QEMU。因此
只验证了主机构建回归、工具链/脚本语法、错误路径和对 x86-64 ELF 的拒绝行为；没有
生成 ARM 二进制，也没有执行 QEMU 或 i.MX6ULL 实机测试。
