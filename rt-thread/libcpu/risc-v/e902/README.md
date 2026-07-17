# E902 CPU移植

该目录对应RT-Thread的E902架构移植。当前已建立同步异常入口、CLIC基础层和统一中断入口：

```text
cpu.mk                   CPU层构建入口
exception_gcc.S          RV32E异常现场保存、恢复和mret
exception.c              异常分类、现场记录和早期诊断输出
interrupt_gcc.S          RV32E中断现场保存、C分发和mret
clic.c                   CLIC初始化、IRQ注册、pending和全局开关
include/e902_exception.h 汇编与C共享的异常/中断现场布局
include/e902_clic.h      CLIC和IRQ分发接口
```

CLIC基础层和统一中断入口已经加入。线程初始栈和上下文切换将在后续阶段加入。

这里只放置CPU、RISC-V CSR、RV32E ABI、异常入口、CLIC核心机制和线程上下文相关内容。

各层职责如下：

- `libcpu/risc-v/e902/`负责E902 CSR、CLIC入口机制、中断开关、线程栈和上下文切换。
- `soc/t22-serdes/`负责T22中断号、系统计数器频率及芯片资源关系。
- `boards/`负责板卡实际启用的设备和控制台选择。
- `drivers/`负责UART、I2C、GPIO和通用Timer等外设控制器。
