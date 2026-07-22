# E902 CPU移植

该目录对应RT-Thread的E902架构移植。当前已建立同步异常入口、CLIC CPU适配、统一中断入口和线程上下文基础层：

```text
cpu.mk                   CPU层构建入口
cpuport.c                线程栈、上下文切换请求、异常分类和早期诊断输出
cpuport_gcc.S            IRQ 3、普通IRQ、异常入口及中断开关汇编实现
e902_irq.c               E902 mtvt安装、mcause解码和通用分发适配
e902_irq.h               E902 IRQ初始化、逻辑level和分发接口
e902.h                   E902现场布局、上下文扩展和异常接口
```

线程上下文与中断入口共用80字节RV32E现场。调度请求只记录`from/to`并置位CLIC IRQ 3；`mtvt[3]`指向专用汇编入口，在普通硬件IRQ返回后完成实际线程SP保存和装载，不经过普通IRQ的C分发。首次线程启动同样由`rt_hw_context_switch_to()`请求IRQ 3，入口以`from == 0`跳过启动栈保存，并通过统一的`mret`路径恢复人工构造的初始现场。

当前port不设置独立中断栈，80字节现场和ISR调用链均使用被中断线程的栈。Board必须按最深ISR路径为应用线程和Idle线程配置足够栈空间，目标板稳定性测试还应覆盖栈高水位。

普通IRQ在现场保存后调用`rt_interrupt_enter()`和`rt_interrupt_leave()`，维护内核中断嵌套状态；IRQ 3对标M3 PendSV，不计入普通ISR嵌套且不调用这两个接口。RT-Thread镜像使用`src/irq.c`强实现；裸机验证应用通过独立测试支持文件提供弱空实现。现场恢复通过汇编宏在各入口独立展开，不设置共享恢复跳转点。

CLIC寄存器访问已经从CPU port抽取到`drivers/interrupt/riscv_clic/`。通用驱动管理显式`struct riscv_clic`实例、CLIC寄存器布局、IRQ属性、单路使能和pending；SoC提供`struct riscv_clic_config`中的基地址、阈值地址和芯片兼容编码；Board持有唯一的RT-Thread `rt_irq_desc`表；E902层只保留`mtvt` CSR安装、E902控制位范围、`mcause`解码及到Board分发入口的转接。该边界避免为CLIC另建一套ISR注册框架。

`e902_irq_init()`接收SoC提供的CLIC配置，并在保持全局中断关闭时调用`riscv_clic_init()`。后者禁用全部硬件IRQ、清除历史pending并配置`CLICCFG`；只有配置提供阈值寄存器地址时才初始化`MINTTHRESH`。E902层随后校验向量容量和`CLICINTCTLBITS`并写入`mtvt`。接口退出时恢复调用者进入前的`mstatus.MIE`状态，初始化失败后不能继续打开中断。

SHV由硬件直接按`mtvt + 4 * irq`取表，因此向量表必须覆盖`CLICINFO`报告的全部硬件IRQ。T22 SoC持有一个CLIC实例和80项向量表，Board持有同等容量的唯一`rt_irq_desc`表；链接脚本验证向量表的对齐和精确大小，E902初始化检查硬件IRQ数不超过向量容量。板级设备驱动仍必须使用T22资源定义中明确分配的IRQ，不能把保留洞或IRQ 71-79当作有效外设中断。

普通设备IRQ先由驱动调用`riscv_clic_configure_irq()`设置硬件触发属性和level，再通过RT-Thread标准`rt_hw_interrupt_install()`安装ISR，并用`rt_hw_interrupt_mask()`/`rt_hw_interrupt_umask()`控制单路使能。普通IRQ入口将`mcause`中的IRQ号交给Board的`rt_hw_interrupt_dispatch()`；IRQ 3继续使用专用汇编入口，不进入普通描述表分发。

这里只放置CPU、RISC-V CSR、RV32E ABI、异常入口、CLIC CPU集成机制和线程上下文相关内容。

各层职责如下：

- `drivers/interrupt/riscv_clic/`负责通用CLIC MMIO和IRQ硬件属性。
- `libcpu/risc-v/e902/`负责E902 CSR、CLIC入口适配、中断开关、线程栈和上下文切换。
- `soc/t22-serdes/`负责T22中断号、系统计数器频率及芯片资源关系。
- `boards/`负责唯一的RT-Thread IRQ描述表、标准ISR安装、普通IRQ分发，以及板卡实际启用的设备和控制台选择。
- `drivers/`负责UART、I2C、GPIO和通用Timer等外设控制器。

通用CLIC驱动使用私有类型化寄存器块，避免把相邻的8位寄存器错误地合并为32位访问；寄存器偏移和步长在编译期检查。T22 SoC和DW APB UART、Timer驱动采用相同原则，寄存器映射分别封装在SoC或对应Driver内部，不进入CPU port公共接口。
