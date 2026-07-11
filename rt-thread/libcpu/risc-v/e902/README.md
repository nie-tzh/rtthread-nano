# E902 CPU移植

该目录对应RT-Thread的E902架构移植，后续包含：

```text
cpuport.c          中断控制和初始线程栈
context_gcc.S      线程上下文切换
interrupt_gcc.S    异常和中断入口
cpuport.h          CPU相关定义
```

这里只放置CPU、RISC-V CSR和ABI相关内容。CLIC寄存器配置、Timer、UART、GPIO等外设实现放入项目`drivers/`目录。
