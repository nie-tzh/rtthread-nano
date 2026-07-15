# RT-Thread Nano T22解串器移植工程

本工程用于在采用玄铁E902 CPU核的T22解串器EVB上移植和运行RT-Thread Nano。

当前已完成目标硬件和工具链确认、最小Makefile编译系统、裸机启动验证、T22解串器EVB板级初始化、DW APB UART早期轮询输出，以及E902同步异常入口与现场恢复验证。固件已完成上板验证，串口可以输出：

```text
T22 deserializer EVB booting...
```

当前第5阶段正在进行：同步异常入口和受控`ebreak`恢复已经验证，下一步是CLIC初始化与Machine Software Interrupt验证。随后实现DW Timer周期中断、线程上下文切换并接入RT-Thread Nano内核。DW Timer将作为RT-Thread系统Tick来源，不使用E902 Core Timer。

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
- [E902异常与现场验证](docs/e902-interrupt-validation.md)
