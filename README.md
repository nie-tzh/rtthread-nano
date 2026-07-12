# RT-Thread Nano T22解串器移植工程

本工程用于在采用玄铁E902 CPU核的T22解串器EVB上移植和运行RT-Thread Nano。

当前已完成目标硬件和工具链确认、最小Makefile编译系统、裸机启动验证、T22解串器EVB板级初始化以及DW APB UART早期轮询输出。固件已完成上板验证，串口可以输出：

```text
T22 deserializer EVB booting...
```

下一阶段将依次建立E902异常入口、CLIC中断机制和DW Timer周期中断，再实现线程上下文切换并接入RT-Thread Nano内核。DW Timer将作为RT-Thread系统Tick来源，不使用E902 Core Timer。

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
