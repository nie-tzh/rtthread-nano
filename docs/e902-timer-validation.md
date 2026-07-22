# E902 DW Timer周期中断验证

## 1. 验证目标

本验证用于确认T22 DW APB Timer、CLIC IRQ 27、E902公共中断入口和SoC Timer分发接口能够形成稳定的裸机周期中断闭环。验证应用与普通`demo`隔离，直接调用T22 Timer接口，不链接RT-Thread板级Tick适配，也不调用`rt_tick_increase()`。

2026-08-07目标板自测最终输出`E902 DW Timer self-test: PASS`，标准IRQ框架下的共享IRQ 27分发、Debug/Release构建和第6阶段功能验收均已通过。

> 本轮目标板日志确认TIMER1周期计数、TIMER2共享IRQ 27、停止隔离、重新启动和最终pending清零均符合预期；长时间漂移、丢Tick和压力测试仍属于第11阶段。

| 验证项 | 目标 |
| --- | --- |
| 周期频率 | TIMER1以1 kHz运行，连续100个周期的误差不超过2% |
| 独立计时 | 使用E902 `mcycle`计数，不使用被测DW Timer估算自身周期 |
| IRQ路径 | 中断经CLIC IRQ 27、硬件向量表和公共入口到达已注册回调 |
| 共享中断 | TIMER2以500 Hz同时运行，两个通道共用IRQ 27且均能持续回调 |
| 停止隔离 | 停止TIMER1后，其计数值和回调次数不再变化，TIMER2仍继续中断 |
| 再次启动 | TIMER1能够重新启动，且不影响仍在运行的TIMER2 |
| 最终清源 | 两个通道停止后，IRQ 27 pending为0，停止隔离检查通过 |

本测试是约130 ms的阶段性功能验证，不代替长时间漂移、丢Tick、压力和低功耗测试。

## 2. 测试资源

| 资源 | 配置 |
| --- | --- |
| 系统Tick通道 | 零基通道0，即TIMER1 |
| 系统Tick频率 | 1000 Hz |
| 辅助通道 | 零基通道1，即TIMER2 |
| 辅助频率 | 500 Hz |
| Timer输入时钟 | 200 MHz APB |
| 周期测量时钟 | 320 MHz CPU `mcycle` |
| 共享中断 | CLIC IRQ 27，高电平触发 |

TIMER2只存在于独立测试应用中，用于验证共享IRQ分发和通道隔离，不属于系统Tick资源。

## 3. 周期测量方法

TIMER1第一次进入回调时记录32位`mcycle`，第101次进入回调时再次记录。两个采样点之间恰好包含100个1 ms周期，因此期望值为：

```text
expected_cycles = 320000000 / 1000 * 100
                = 32000000
                = 0x01E84800
```

允许误差为期望值的2%，即`640000`个CPU周期。测试窗口只有约100 ms，小于32位`mcycle`在320 MHz下约13.4秒的回绕周期；使用无符号减法，即使两个采样点跨过一次回绕仍可得到正确差值。

`mcycle`测得的是CPU执行周期。测试期间主程序持续忙等，不进入低功耗状态，因此它可以作为独立于DW Timer的短时参考时钟。

## 4. 测试流程

1. 保持`mstatus.MIE=0`，调用`rt_hw_interrupt_init()`完成CLIC、向量表和板级RT-Thread IRQ描述表初始化。
2. 调用`t22_serdes_timer_init()`初始化8个DW Timer通道，并通过`rt_hw_interrupt_install()`注册共享IRQ 27处理函数。
3. 通过`t22_serdes_timer_config_periodic()`将TIMER1配置为1 kHz、TIMER2配置为500 Hz，并分别注册验证回调。
4. 依次启动TIMER2和TIMER1，最后打开全局中断。
5. 等待TIMER1完成101次回调，使用第1次和第101次回调时间计算100个周期的总误差。
6. 关闭全局中断并停止TIMER1，记录其当前计数值和回调次数。
7. 重新打开全局中断，等待TIMER2继续完成5次回调；确认TIMER1计数值和回调次数均未变化。
8. 再次启动TIMER1，等待其继续完成20次回调，同时确认TIMER2仍在运行。
9. 依次停止TIMER1和TIMER2，确认IRQ 27 pending为0。
10. 检查回调参数、通道号、停止后的计数和pending状态，全部满足条件后输出`PASS`。

等待过程同时检查`mcycle`超时和软件循环上限，避免中断未到达或性能计数器停止时永久卡在等待循环。

## 5. 构建与运行

在WSL中执行：

当前通过WSL调用Windows原生玄铁GCC时，使用相对`BUILD_DIR`；`O`仍兼容纯Linux工具链。

```sh
make BOARD=t22-deserializer-evb APP=e902-timer-test BUILD=debug \
     BUILD_DIR=build/t22-deserializer-evb/e902-timer-test/debug
```

直接运行时，将`firmware.bin`下载到`0x00140000`后复位，观察UART日志。

使用CKLink时，先启动XuanTie DebugServer，再在VS Code中选择`E902 DW Timer test | CKLink`。该启动项只负责复位、下载和调试，不触发WSL编译，并保持普通调试断点由Debug Mode接管。

## 6. 历史实测结果与当前日志格式

目标板连续运行后应最终输出`E902 DW Timer self-test: PASS`。周期计数和回调次数会因中断相位略有差异，日志结构为：

```text
T22 deserializer EVB booting...
E902 DW Timer self-test: init
E902 DW Timer self-test: tick_hz=0x000003E8 aux_hz=0x000001F4
E902 DW Timer self-test: cycles=0x........ expected=0x01E84800 tolerance=0x0009C400
E902 DW Timer self-test: tick_count=0x........ aux_count=0x........
E902 DW Timer self-test: stopped_current=0x......../0x........ pending=0x00000000
E902 DW Timer self-test: PASS
```

2026-08-07目标板实测日志为：

```text
E902 DW Timer self-test: init
E902 DW Timer self-test: tick_hz=0x000003E8 aux_hz=0x000001F4
E902 DW Timer self-test: cycles=0x01E84844 expected=0x01E84800 tolerance=0x0009C400
E902 DW Timer self-test: tick_count=0x00000079 aux_count=0x00000041
E902 DW Timer self-test: stopped_current=0x00000000/0x00000000 pending=0x00000000
E902 DW Timer self-test: PASS
```

该次实测周期为`32000068`个CPU周期，相对期望值`32000000`多`68`个周期。在320 MHz下对应约`0.213 us`，100 ms测量窗口的相对误差约为`0.000213%`。最终TIMER1回调`121`次、辅助回调`65`次；停止TIMER1后两次读取的当前计数值均为0，最终IRQ pending为0。

测试程序只有在全部判定通过时才会输出`PASS`：`cycles`位于`0x01DE8400`到`0x01F20C00`之间，`tick_count`至少为121，`aux_count`大于0，两个`stopped_current`相等，最终`pending`为0，且回调参数、通道号和停止隔离状态均正确。因此本轮`PASS`确认了1 kHz周期精度、IRQ 27共享分发、停止隔离、再次启动和最终清源路径；长时间漂移和压力测试仍属于第11阶段。

## 7. 失败码

失败日志格式为：

```text
E902 DW Timer self-test: FAIL result=0x........ status=0x........
```

`status`保留最近一次底层接口返回值；纯行为校验失败时通常为0。

| `result` | 失败位置 | 优先检查项 |
| --- | --- | --- |
| 1 | Timer模块初始化或TIMER1配置 | IRQ 27标准安装、Timer初始化状态、频率和TIMER1回调参数 |
| 2 | TIMER2配置 | 通道号、500 Hz整除关系和回调注册 |
| 3 | TIMER2启动 | 通道配置状态和IRQ 27使能 |
| 4 | TIMER1启动 | TIMER1配置状态、共享活动掩码和驱动启动 |
| 5 | 首轮等待超时 | `mstatus.MIE`、CLIC IRQ 27、Timer装载值、公共状态和EOI |
| 6 | 周期误差超限 | 200 MHz APB、320 MHz CPU时钟、装载值或中断延迟异常 |
| 7 | TIMER1停止失败 | SoC到Driver的停止路径 |
| 8 | TIMER1当前值读取失败 | 通道初始化状态和当前计数寄存器 |
| 9 | TIMER1停止后TIMER2不再中断 | 共享活动掩码错误地关闭了IRQ 27 |
| 10 | TIMER1停止行为异常 | TIMER1仍在计数或仍调用回调 |
| 11 | TIMER1再次启动失败 | 停止后的通道状态、EOI和活动掩码 |
| 12 | 再次启动后等待超时 | TIMER1未恢复或TIMER2被意外停止 |
| 13 | TIMER1最终停止失败 | SoC停止路径和Timer控制寄存器 |
| 14 | TIMER2最终停止失败 | SoC共享Timer停止路径 |
| 15 | 最终状态不一致 | pending、回调参数、通道号或停止状态 |

## 8. 看门狗与调试注意事项

测试应用不初始化、关闭或喂T22看门狗，避免把看门狗策略混入Timer验证。参考裸机工程将看门狗超时配置为约5.3秒，而本测试的正常执行时间约130 ms。若当前启动链提前启用了更短的看门狗，测试中途复位时应先确认复位原因，再决定由板级启动策略统一处理看门狗，不能在Timer ISR中临时喂狗掩盖问题。

自测输出`PASS`后会停在空循环中，仍不会喂看门狗。因此，`PASS`之后才出现的WDT NMI或复位不推翻已经完成的短时Timer验证；它说明后续正式系统仍需建立统一的看门狗策略。若WDT在`PASS`之前动作，则本次结果无效，应先查清启动链中的实际超时配置。

硬件断点会让CPU停止而外设是否继续计数取决于T22调试暂停配置。单步调试可能破坏周期测量，因此频率结果应以连续运行时的UART日志为准。

## 9. 代码对应关系

| 内容 | 文件 |
| --- | --- |
| 独立Timer验证应用 | `apps/e902-timer-test/main.c` |
| RT-Thread板级Tick适配 | `boards/t22-deserializer-evb/board.c` |
| T22共享Timer分发 | `soc/t22-serdes/t22_serdes_timer.c` |
| DW APB Timer通用驱动 | `drivers/timer/dw_apb_timer/dw_apb_timer.c` |
| 通用CLIC、E902适配和公共入口 | `drivers/interrupt/riscv_clic/riscv_clic.c`、`rt-thread/libcpu/risc-v/e902/e902_irq.c`、`cpuport_gcc.S` |
| CKLink启动配置 | `.vscode/launch.json` |
