# RT-Thread Nano T22解串器移植工程

本工程用于在采用玄铁E902 CPU核的T22解串器EVB上移植和运行RT-Thread Nano。

当前已完成目标硬件和工具链确认、最小Makefile编译系统、裸机启动验证、T22解串器EVB板级初始化、DW APB UART早期轮询输出，以及E902同步异常入口、CLIC初始化、Machine Software Interrupt和DW Timer周期中断上板验证。基础固件的串口可以输出：

```text
T22 deserializer EVB booting...
```

第6、7、8阶段已经完成：T22 DW Timer周期时基、RV32E线程上下文、RT-Thread Nano调度器和TIMER1系统Tick均已通过目标板验证。下一步进入第9阶段，将板级外设逐步接入RT-Thread Device框架；系统Tick继续固定使用TIMER1，不使用E902 Core Timer。

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

默认产物位于：

```text
build/t22-deserializer-evb/demo/debug/
```

其中包含`firmware.elf`、`firmware.bin`、`firmware.map`和`firmware.lst`。

相关文档：

- [工程架构](docs/architecture.md)
- [移植进度](docs/porting-progress.md)
- [E902异常与CLIC中断架构](docs/e902-interrupt-architecture.md)
- [E902异常与CLIC验证](docs/e902-interrupt-validation.md)
- [E902 DW Timer周期中断验证](docs/e902-timer-validation.md)
- [E902线程上下文切换验证](docs/e902-context-switch-validation.md)
- [E902 RT-Thread调度与Tick验证](docs/e902-rtthread-validation.md)
