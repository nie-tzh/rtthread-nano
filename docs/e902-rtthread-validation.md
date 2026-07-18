# E902 RT-Thread调度与Tick验证

## 1. 验证目标与状态

本验证用于确认RT-Thread v4.1.1 Nano内核已经与E902 CPU port和T22 DW Timer形成完整闭环。测试代码位于独立应用中，不改变普通`demo`及前序裸机验证应用。

当前已经完成Debug、Release构建、严格告警、ELF属性、强弱中断符号选择、反汇编检查和目标板验证，第8阶段已完成。目标板最终输出`E902 RT-Thread self-test: PASS`。

| 验证项 | 目标 |
| --- | --- |
| 首次调度 | `rt_system_scheduler_start()`选择线程A并通过初始现场进入线程入口 |
| 时间片 | 两个同优先级、1 Tick时间片线程由系统Tick轮转 |
| 线程延时 | `rt_thread_delay()`挂起当前线程并由内核定时器唤醒 |
| Idle过渡 | 两个线程同时延时时能够运行Idle，线程超时后重新抢占 |
| 系统Tick | TIMER1回调次数与`rt_tick_get()`完全一致 |
| IRQ边界 | `rt_interrupt_nest`在线程态回到0，IRQ 3完成延后切换 |
| 栈完整性 | 两个1024字节线程栈底部64字节保持填充值`'#'` |
| 最终状态 | IRQ 3/27 pending和切换标志为0，无未处理IRQ或CPU port错误 |

## 2. 最小内核配置

板级`rtconfig.h`使用以下裁剪：

```text
32级线程优先级
1000 Hz系统Tick
4字节ABI对齐
启用线程栈溢出检查
禁用Heap、组件自动初始化、设备框架和FinSH
```

当前阶段使用静态线程对象、静态线程栈和RT-Thread自带静态Idle线程。该配置只用于先验证调度内核，不代表后续应用框架最终配置。

## 3. 初始化顺序

测试主函数始终保持`mstatus.MIE=0`并依次执行：

```text
rt_hw_interrupt_init()
    -> rt_system_timer_init()
    -> rt_system_scheduler_init()
    -> rt_thread_init(A/B)
    -> rt_thread_startup(A/B)
    -> rt_thread_idle_init()
    -> board_tick_init(1000 Hz)
    -> board_tick_start()
    -> rt_system_scheduler_start()
```

Timer启动后不在`main()`中提前开放MIE。`rt_system_scheduler_start()`装载线程A初始现场，第一次`mret`才恢复`MPIE`并开放中断，保证第一个Tick到达时`rt_current_thread`已经有效。

## 4. IRQ边界所有权

E902公共IRQ入口统一执行：

```text
保存RV32E现场
    -> rt_interrupt_enter()
    -> CLIC处理函数
    -> rt_interrupt_leave()
    -> 检查IRQ 3切换请求
    -> 恢复现场并mret
```

因此TIMER1回调只增加测试计数并调用一次`rt_tick_increase()`。若回调再次调用`rt_interrupt_enter/leave`，单层IRQ会被错误记录为两层，破坏中断嵌套统计和进入、退出Hook的语义。

最终ELF中的`rt_interrupt_enter/leave`来自`rt-thread/src/irq.c`强实现；不含内核的应用仍使用`rtinterrupt.c`弱空实现。

## 5. 时间片与延时流程

线程A和B优先级均为5，初始时间片均为1 Tick。每个线程先执行4轮忙等Tick变化：

```text
A等待Tick -> Tick ISR耗尽A时间片 -> 调度B
B等待Tick -> Tick ISR耗尽B时间片 -> 调度A
```

两个线程均完成4轮后，再分别执行4次延时：

```text
A: rt_thread_delay(2)
B: rt_thread_delay(3)
```

当两个线程同时延时时，Idle线程成为最高优先级就绪线程。线程定时器超时发生在`rt_tick_increase()`末尾的`rt_timer_check()`中，超时线程重新进入就绪队列，并在当前IRQ结束后通过IRQ 3完成抢占。

## 6. 构建与运行

在WSL中执行：

```sh
make BOARD=t22-deserializer-evb APP=e902-rtthread-test BUILD=debug \
     O=build/t22-deserializer-evb/e902-rtthread-test/debug
```

直接运行时，将`firmware.bin`下载到`0x00140000`后复位并观察UART。

使用CKLink时，先启动XuanTie DebugServer，再在VS Code中选择`E902 RT-Thread test | CKLink`。该配置只负责复位、下载和调试，不触发WSL编译。

## 7. 通过日志

`ticks`和`switches`取决于实际调度时序，不要求固定值，但必须满足测试代码中的下限和一致性检查：

本次目标板验证满足以下全部条件：

- A/B时间片计数均为4。
- A/B延时完成计数均为4。
- TIMER1回调和`rt_tick_get()`均为21，CPU port完成31次实际上下文切换。
- `rt_interrupt_nest`在线程态为0。
- IRQ 3和IRQ 27 pending、切换标志和CPU port错误状态均为0。
- 两个线程栈底部哨兵保持不变。
- 串口最终输出`E902 RT-Thread self-test: PASS`。

```text
T22 deserializer EVB booting...
E902 RT-Thread self-test: init
E902 RT-Thread self-test: scheduler start
E902 RT-Thread self-test: ticks=0x00000015 switches=0x0000001F slice_A/B=0x00000004/0x00000004 delay_A/B=0x00000004/0x00000004 irq_nest=0x00000000
E902 RT-Thread self-test: PASS
```

该结果在链接`src/irq.c`强`rt_interrupt_enter/leave`实现的内核镜像上取得。反汇编确认公共IRQ入口在`rt_interrupt_enter()`返回后重新由`sp`装载分发参数`a0`，因此本次PASS同时覆盖了ILP32E调用者保存寄存器约束下的IRQ现场传递。

## 8. 失败码

失败日志格式：

```text
E902 RT-Thread self-test: FAIL result=0x........ status=0x........
```

| `result` | 失败位置 | 优先检查项 |
| --- | --- | --- |
| 1 | CLIC、上下文或Board Tick初始化 | 初始化顺序、IRQ 3/27注册和TIMER1配置 |
| 2 | 静态线程初始化 | `rtconfig.h`、线程对象、栈地址和初始现场 |
| 3 | 调度器启动意外返回 | 就绪队列、首次线程选择和`rt_hw_context_switch_to()` |
| 4 | Tick或上下文切换计数 | TIMER1回调、`rt_tick_increase()`、时间片和IRQ 3 |
| 5 | A/B阶段次数不一致 | 同优先级轮转、延时返回和线程定时器唤醒 |
| 6 | IRQ最终状态异常 | pending、切换标志、嵌套计数或未处理IRQ |
| 7 | 线程栈哨兵损坏 | 栈大小、IRQ/C调用深度或现场偏移 |

`status`保存初始化接口返回值；后续逻辑失败时为0。失败路径关闭全局中断并停机，避免失败后继续被Tick调度。

## 9. 验证边界

本测试证明静态线程、优先级调度、时间片、硬定时器和线程延时已经接入，但不覆盖：

- 动态Heap和动态线程创建。
- 信号量、互斥量、事件、邮箱和消息队列。
- RT-Thread Device框架和UART设备驱动。
- 组件自动初始化、用户Main线程和FinSH。
- 中断嵌套、长时间Tick漂移和压力稳定性。

这些内容分别属于第9阶段驱动接入、第10阶段应用框架和第11阶段稳定性验证。

## 10. 代码对应关系

| 内容 | 文件 |
| --- | --- |
| 最小内核源文件集合 | `rt-thread/rtthread.mk` |
| Nano配置 | `boards/t22-deserializer-evb/include/rtconfig.h` |
| 板级RT中断适配 | `boards/t22-deserializer-evb/rtthread.c` |
| IRQ中断边界 | `rt-thread/libcpu/risc-v/e902/interrupt_gcc.S`、`rtinterrupt.c` |
| A/B调度和PASS/FAIL判断 | `apps/e902-rtthread-test/main.c` |
| CKLink配置 | `.vscode/launch.json` |
