# RT-Thread Nano T22解串器移植工程

本工程用于在采用玄铁E902 CPU核的T22解串器EVB上移植和运行RT-Thread Nano。

前期移植已经完成目标硬件和工具链确认、最小Makefile编译系统、裸机启动验证、T22解串器EVB板级初始化和DW APB UART早期轮询输出；当前版本已完成E902同步异常入口、CLIC初始化、Machine Software Interrupt、DW Timer周期中断、线程上下文切换和RT-Thread Nano调度的目标板验证。基础固件的串口可以输出：

```text
T22 deserializer EVB booting...
```

第6、7、8阶段的功能验证已在当前版本完成：T22 DW Timer周期时基、RV32E线程上下文、RT-Thread Nano调度器和TIMER1系统Tick均已通过目标板验证。下一步进入第9阶段，将板级外设逐步接入RT-Thread Device框架；系统Tick继续固定使用TIMER1，不使用E902 Core Timer。

> 当前版本已完成生产化整理后的构建和目标板复测。异常、CLIC、DW Timer、线程上下文切换和RT-Thread调度测试均输出`PASS`；长时间Tick漂移、丢Tick、高负载和栈高水位测试仍属于第11阶段。

默认构建目标由板卡配置自动绑定为：

```text
BOARD=t22-deserializer-evb
SOC=t22-serdes
CHIP=t22-deserializer
CPU=e902
```

通常只需要选择`BOARD`，不应手工组合不匹配的SoC、芯片和CPU。

## 编译环境

- WSL Linux
- 玄铁GCC裸机工具链
- 工具链前缀：`riscv64-unknown-elf-`
- CPU参数：`-mcpu=e902 -mabi=ilp32e -mcmodel=medlow`

工具链通过`PATH`查找，工程中不使用绝对路径。

## 构建

```shell
make
make BUILD=release
make info
make clean
```

通过WSL调用Windows原生玄铁GCC时，建议使用相对`BUILD_DIR`指定独立输出目录，例如`BUILD_DIR=build/t22-deserializer-evb/demo/debug`。`O`仍兼容纯Linux工具链；避免在混合环境中让`O`被展开为`/mnt/...`后传给Windows工具链。

默认产物位于：

```text
build/t22-deserializer-evb/demo/debug/
```

其中包含`firmware.elf`、`firmware.bin`、`firmware.map`和`firmware.lst`。

相关文档：

- [工程架构](docs/architecture.md)
- [移植进度](docs/porting-progress.md)
- [Pinctrl子系统设计、Linux实现与T22落地](docs/pinctrl-subsystem-design.md)
- [Reset子系统设计与T22实施方案](docs/reset-subsystem-design.md)
- [Clock子系统设计、Linux CCF与T22落地](docs/clock-subsystem-design.md)
- [E902异常与CLIC中断架构](docs/e902-interrupt-architecture.md)
- [E902异常与CLIC验证](docs/e902-interrupt-validation.md)
- [E902 DW Timer周期中断验证](docs/e902-timer-validation.md)
- [E902线程上下文切换验证](docs/e902-context-switch-validation.md)
- [E902 RT-Thread调度与Tick验证](docs/e902-rtthread-validation.md)
