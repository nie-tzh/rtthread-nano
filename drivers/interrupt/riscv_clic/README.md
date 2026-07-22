# RISC-V CLIC驱动

该目录提供与具体CPU异常入口解耦的CLIC寄存器驱动。

当前公共层覆盖T22/E902所用的CLIC MMIO轮廓：`CLICCFG`位于`0x0`、`CLICINFO`位于`0x4`，每路IP/IE/ATTR/CTL从`0x1000`开始并以4字节为步长。阈值位置、`num_interrupt=0`的兼容含义、CPU向量CSR和入口协议不属于这组固定布局，分别由SoC配置或CPU适配层提供。接入其他CLIC版本时必须先核对这些边界，不能仅凭控制器名称直接复用。

```text
riscv_clic.c             CLIC MMIO布局、初始化和IRQ硬件配置
include/riscv_clic.h     CLIC实例、配置、硬件信息和公共接口
driver.mk                工程构建入口
```

调用者持有`struct riscv_clic`实例，并通过`struct riscv_clic_config`提供CLIC基地址、可选的阈值寄存器地址以及`CLICINFO.num_interrupt=0`时的兼容IRQ数量。通用驱动不持有ISR描述表，也不负责处理函数注册和C分发；这些职责由操作系统的中断框架承担。驱动从`CLICINFO`取得硬件IRQ数量与`CLICINTCTLBITS`，禁用并清理所有已实现IRQ，令全部有效控制位作为逻辑level使用；若配置提供阈值地址，则将该地址初始化为0。

当前驱动统一使用Selective Hardware Vectoring：配置IRQ时设置`CLICINTATTR.shv=1`，并按调用者给出的触发方式和逻辑level写入`CLICINTATTR`与`CLICINTCTL`。`CLICCFG`只修改`nlbits`，`CLICINTATTR`只修改`shv/trig`，其余实现字段保持原值。CPU相关代码必须另外提供硬件向量表、安装`mtvt`并从`mcause`取得IRQ号；这些操作不属于通用CLIC MMIO驱动。

快速寄存器接口不重复检查实例和IRQ号。调用顺序必须保证先成功执行`riscv_clic_init()`，再配置或使能IRQ。`riscv_clic_configure_irq()`在全局中断关闭的临界区内关闭目标IRQ、清除CLIC pending，并写入触发属性和逻辑level；ISR安装及单路开关应使用上层操作系统提供的标准接口。
