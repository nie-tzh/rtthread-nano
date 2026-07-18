# E902 CPU移植

该目录对应RT-Thread的E902架构移植。当前已建立同步异常入口、CLIC基础层、统一中断入口和线程上下文基础层：

```text
cpu.mk                   CPU层构建入口
context.c                线程初始栈、IRQ 3注册和切换请求合并
context_gcc.S            首次线程恢复和RT-Thread中断开关接口
exception_gcc.S          RV32E异常现场保存、恢复和mret
exception.c              异常分类、现场记录和早期诊断输出
interrupt_gcc.S          RV32E中断现场保存、IRQ 3切换、恢复和mret
rtinterrupt.c            裸机兼容的RT-Thread中断边界弱空实现
clic.c                   CLIC初始化、IRQ注册、pending和全局开关
include/e902_context.h   线程上下文接口、状态和RT-Thread移植契约
include/e902_exception.h 汇编与C共享的异常/中断现场布局
include/e902_clic.h      CLIC和IRQ分发接口
```

线程上下文与中断入口共用80字节RV32E现场。普通调度请求只记录`from/to`并置位CLIC IRQ 3；IRQ 3统一入口完成实际线程SP保存和装载。首次线程启动由`rt_hw_context_switch_to()`直接恢复人工构造的初始现场。

必要代码已完成构建和反汇编检查，独立双线程应用已经通过目标板验证。第8阶段的RT-Thread镜像使用`src/irq.c`强实现覆盖`rtinterrupt.c`弱实现，使同一个公共IRQ入口同时兼容裸机验证和内核中断嵌套计数。

这里只放置CPU、RISC-V CSR、RV32E ABI、异常入口、CLIC核心机制和线程上下文相关内容。

各层职责如下：

- `libcpu/risc-v/e902/`负责E902 CSR、CLIC入口机制、中断开关、线程栈和上下文切换。
- `soc/t22-serdes/`负责T22中断号、系统计数器频率及芯片资源关系。
- `boards/`负责板卡实际启用的设备和控制台选择。
- `drivers/`负责UART、I2C、GPIO和通用Timer等外设控制器。
