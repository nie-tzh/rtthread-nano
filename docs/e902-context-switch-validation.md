# E902线程上下文切换验证

## 1. 验证目标与状态

本验证用于在尚未接入RT-Thread调度器和系统Tick前，独立检查E902 CPU port的线程初始栈、首次线程启动和CLIC IRQ 3延后上下文切换。测试代码位于独立应用中，不改变普通`demo`的执行流程。

当前验证应用已经完成Debug、Release构建、严格告警、ELF属性、反汇编检查和目标板验证，第7阶段已完成。目标板串口输出与本文预期日志一致。

| 验证项 | 目标 |
| --- | --- |
| 初始现场 | `ra/sp/gp/a0/mepc/mstatus/mcause`与线程入口契约一致 |
| 首次启动 | `rt_hw_context_switch_to()`通过初始现场和`mret`进入线程A |
| 请求合并 | IRQ 3 pending期间保留第一个真实`from`，只更新最终`to` |
| 往返切换 | 线程A/B分别使用普通和中断态接口完成多轮切换 |
| GPR完整性 | 验证RV32E `x1-x15`，其中`sp`检查范围、`gp`检查全局指针，其余使用特征值 |
| CSR返回 | 首次启动装载初始`mcause`；IRQ 3切换保留当前`mcause.MPIL`并恢复目标线程的`mstatus/mepc` |
| 栈完整性 | 两个独立线程栈底部64字节哨兵保持不变 |
| 最终状态 | pending和切换标志为0，没有未处理IRQ或线程异常退出 |

## 2. 测试结构

测试应用建立两个1024字节栈和两个初始线程现场：

```text
main
    -> 初始化T22 CLIC
    -> 初始化E902上下文切换IRQ 3
    -> 构造线程A和线程B初始现场
    -> 检查初始现场字段
    -> rt_hw_context_switch_to(A)

线程A
    -> 验证首次入口参数
    -> 验证两次请求合并为一次A->A现场保存恢复
    -> 装载寄存器特征值并切换到B
    -> 与B往返8轮
    -> 汇总结果并输出PASS/FAIL

线程B
    -> 验证首次入口参数
    -> 装载寄存器特征值并切换回A
    -> 使用rt_hw_context_switch_interrupt()与A往返
```

线程入口均设置统一的退出函数。正常测试不会返回入口；若错误返回，退出函数记录失败码并停机。

## 3. 初始现场检查

`rt_hw_stack_init()`返回后、第一次`mret`前，主程序直接检查两个80字节现场：

| 字段 | 期望值 |
| --- | --- |
| `ra` | 统一线程退出函数 |
| `sp` | 按4字节对齐的线程栈顶 |
| `gp` | `__global_pointer$` |
| `tp` | 0 |
| `a0` | 各线程参数地址 |
| `mepc` | 各线程入口地址 |
| `mstatus` | `MPP=M, MPIE=1`，即`0x00001880` |
| `mcause` | 0，由`rt_hw_context_switch_to()`装载，使首次`mret`恢复到CLIC level 0 |
| `mtval/reserved` | 0 |

线程A能够以正确参数执行，进一步证明首次恢复设置的`sp/gp/a0/mepc/mstatus/mcause`已经生效。

## 4. 请求合并验证

线程A先关闭全局中断，连续发出：

```text
A -> B  使用rt_hw_context_switch()
B -> A  使用rt_hw_context_switch_interrupt()
```

第二次请求发生时IRQ 3尚未处理。正确实现必须继续保留真实运行线程A的`from`字段地址，只把最终`to`更新为A。重新开中断后，IRQ 3保存A并重新装载A，相当于一次可验证的现场往返，但线程B不应启动。

该阶段通过条件为：

- 线程A入口次数为1，线程B入口次数为0。
- 请求计数为2。
- IRQ 3和实际切换计数均为1。

如果错误地把`from`覆盖为B，A的运行现场会写入B的初始栈指针字段，随后A可能从初始入口重新启动；入口次数检查会立即失败。

## 5. 寄存器与往返验证

汇编辅助函数在`mstatus.MIE=0`时先提交切换请求，再装载寄存器特征值；打开MIE前执行`fence iorw, iorw`，保证CLIC pending写入已经对CPU可见。目标线程切换回来后，辅助函数先把全部寄存器保存到快照，再使用临时寄存器复制结果。

检查内容为：

- `ra`和`tp/t0-t2/s0-s1/a0-a5`等于各自特征值。
- `sp`仍位于当前线程栈有效范围且满足4字节对齐。
- `gp`仍等于`__global_pointer$`，保证C代码的小数据访问有效。

随后A/B再往返8轮。A使用`rt_hw_context_switch()`，B使用`rt_hw_context_switch_interrupt()`，以同一测试覆盖Nano内核的两个非SMP接口。

最终理论计数为：

```text
request_count = 2 + 2 + 8 * 2 = 20 = 0x14
irq_count     = 1 + 2 + 8 * 2 = 19 = 0x13
switch_count  = 19                  = 0x13
```

第一项中的2次请求来自合并测试，但只产生1次IRQ和实际现场恢复。测试结束时线程A已经从第8次切换返回；线程B刚发出最后一次`B -> A`请求并处于挂起状态，因此线程返回计数预期为`A=8/8, B=8/7`。

## 6. 构建与运行

在WSL中执行：

```sh
make BOARD=t22-deserializer-evb APP=e902-context-switch-test BUILD=debug \
     O=build/t22-deserializer-evb/e902-context-switch-test/debug
```

直接运行时，将对应`firmware.bin`下载到`0x00140000`后复位并观察UART。

使用CKLink时，先启动XuanTie DebugServer，再在VS Code中选择`E902 context switch test | CKLink`。该配置只负责复位、下载和调试，不触发WSL编译。周期测量不参与本测试，可以正常使用硬件断点；但不要在IRQ 3保存一半时手工修改`sp`或切换请求全局变量。

## 7. 实测日志

目标板验证通过，实际串口日志如下：

```text
T22 deserializer EVB booting...
E902 context self-test: init
E902 context self-test: start first thread
E902 context self-test: requests=0x00000014 irqs=0x00000013 switches=0x00000013
E902 context self-test: A=0x00000008/0x00000008 B=0x00000008/0x00000007 pending=0x00000000
E902 context self-test: PASS
```

修正前曾出现以下特征：`requests=0x0000000C`、`irqs=0x00000002`、`switches=0x00000002`、线程B完成8次请求、IRQ 3保持pending且最终返回失败码13。该现象说明A切换到B后，同level的IRQ 3不能再次被CPU接受。根因是切换`sp`后从B的初始现场装载了`mcause=0`，覆盖了当前IRQ 3用于`mret`的`MPIL`。当前实现只在普通IRQ返回同一现场时恢复保存的`mcause`；IRQ 3切换线程时保留当前CSR中的`mcause`。上述19次连续切换和最终`pending=0`验证了修正结果。

## 8. 失败码

失败日志格式为：

```text
E902 context self-test: FAIL result=0x........ context_error=0x........
```

| `result` | 失败位置 | 优先检查项 |
| --- | --- | --- |
| 1 | T22 CLIC初始化 | `CLICINFO`、向量表和全局中断初始状态 |
| 2 | 上下文IRQ初始化 | IRQ 3注册、正边沿属性、pending清除和单路使能 |
| 3 | 初始栈构造 | 栈顶参数、对齐、现场大小和返回指针 |
| 4 | 初始现场字段 | `ra/sp/gp/a0/mepc/mstatus/mcause` |
| 5 | 线程A首次启动异常 | 初始`mret`、入口地址、参数或请求合并覆盖了A现场 |
| 6 | 请求合并 | 第一个`from`被覆盖、最终`to`错误、IRQ 3未到达或重复处理 |
| 7 | 线程A寄存器 | IRQ 3保存/恢复偏移、CSR恢复或辅助汇编顺序 |
| 8 | 线程B寄存器 | B初始现场、A到B切换或B恢复路径 |
| 9 | 次数不一致 | 请求合并、重复IRQ、丢失pending或往返流程错误 |
| 10 | 栈哨兵/指针 | 上下文帧大小、C调用栈、SP保存或栈空间不足 |
| 11 | 最终状态 | pending、切换标志、IRQ统计或CPU port错误状态 |
| 12 | 线程入口返回 | 线程函数意外执行到返回路径 |
| 13 | 不应返回的接口返回 | 首次启动返回main或线程B完成全部循环；若IRQ 3保持pending且计数停止增长，检查`mcause.MPIL`是否被目标现场覆盖 |

`context_error`来自CPU port：`0xFFFFFFFF`表示上下文状态错误，`0xFFFFFFFE`表示CLIC IRQ操作失败。

## 9. 验证边界

本测试证明CPU port能够在两个裸机测试线程之间保存和恢复现场，但不证明RT-Thread调度器已经接入。以下内容留到第8阶段或稳定性阶段：

- `rt_interrupt_enter()`和`rt_interrupt_leave()`边界。
- 就绪队列、优先级抢占和时间片轮转。
- DW Timer回调中的`rt_tick_increase()`。
- 真正硬件ISR唤醒高优先级线程后的延后调度。
- 中断嵌套、长时间压力和线程栈水位统计。

测试结束后停在空循环，不处理看门狗。若`PASS`之后出现WDT NMI或复位，不推翻已经完成的上下文短时验证；若WDT在`PASS`前动作，本次结果无效。

## 10. 代码对应关系

| 内容 | 文件 |
| --- | --- |
| 双线程流程和PASS/FAIL判断 | `apps/e902-context-switch-test/main.c` |
| GPR特征值装载和恢复后快照 | `apps/e902-context-switch-test/context_self_test_gcc.S` |
| 初始栈和请求合并 | `rt-thread/libcpu/risc-v/e902/context.c` |
| 首次线程恢复 | `rt-thread/libcpu/risc-v/e902/context_gcc.S` |
| IRQ 3实际SP切换和统一恢复 | `rt-thread/libcpu/risc-v/e902/interrupt_gcc.S` |
| CLIC注册、pending与分发 | `rt-thread/libcpu/risc-v/e902/clic.c` |
| CKLink启动配置 | `.vscode/launch.json` |
