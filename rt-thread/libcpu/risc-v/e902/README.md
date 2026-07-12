# E902 CPU移植

该目录对应RT-Thread的E902架构移植，后续包含：

```text
cpuport.c          中断控制和初始线程栈
context_gcc.S      线程上下文切换
interrupt_gcc.S    异常和中断入口
cpuport.h          CPU相关定义
```

这里只放置CPU、RISC-V CSR、RV32E ABI、异常入口、CLIC核心机制和线程上下文相关内容。

各层职责如下：

- `libcpu/risc-v/e902/`负责E902 CSR、CLIC入口机制、中断开关、线程栈和上下文切换。
- `soc/t22-serdes/`负责T22中断号、系统计数器频率及芯片资源关系。
- `boards/`负责板卡实际启用的设备和控制台选择。
- `drivers/`负责UART、I2C、GPIO和通用Timer等外设控制器。
