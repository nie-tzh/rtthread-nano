# E902异常与现场验证

本文记录T22 E902异常入口的验证方法、构建方式、预期结果和失败分析。它与[《E902异常与CLIC中断架构》](e902-interrupt-architecture.md)分工如下：

- 架构文档解释硬件机制、寄存器关系、现场边界和默认异常策略。
- 本文解释如何通过受控测试证明异常入口和现场恢复实现正确。

测试代码通过独立的`e902-exception-test`应用显式启用，默认Debug和Release固件不执行主动异常测试。

## 1. 验证范围

当前验证覆盖以下内容：

| 验证项 | 目标 |
| --- | --- |
| 异常入口 | `mtvec`指向E902公共异常入口，入口能够保存RV32E现场 |
| CSR现场 | 正确保存`mepc`、`mstatus`、`mcause`和`mtval` |
| GPR现场 | `x1-x15`、`sp`和`gp`在异常返回后保持正确 |
| 栈边界 | 异常栈帧不越界、不破坏哨兵 |
| 断点恢复 | 受控`ebreak`能够按指令长度修改`mepc`并执行`mret` |
| 异常隔离 | 未启用测试开关时，异常只诊断并停机 |
| CKLink兼容性 | 调试器连接时，受控`ebreak`仍进入异常入口，源码断点仍可由硬件断点停住 |

当前自测使用`.option norvc`生成32位`ebreak`。`exception.c`同时包含对`c.ebreak`的长度识别逻辑，但压缩断点的独立执行用例应单独增加，不能仅凭代码阅读视为已经完成验证。

## 2. 前置条件

### 2.1 共通前置条件

1. 目标为T22解串器EVB，启动代码使用E902异常入口。
2. 早期UART已经初始化，异常输出接口可用。
3. `mtvec.BASE`满足64字节对齐，链接脚本的对齐断言通过。
4. 测试使用独立输出目录，不覆盖普通Debug或Release构建产物。
5. 自测函数中的`ebreak`地址不应被另一个源码断点覆盖。

### 2.2 直接下载或脱离调试器验证

将异常自测固件下载后直接运行时，目标不能处于会把`ebreak`截获到Debug Mode的调试状态。此时`ebreak`应作为M模式Breakpoint异常进入`mtvec`，由异常入口记录现场、前移`mepc`并通过`mret`返回。

### 2.3 CKLink与XuanTie DebugServer验证

仓库提供的`E902 exception test | CKLink`启动配置用于在连接CKLink时执行同一份自测。它在下载固件后执行：

```gdb
monitor set resume-bkpt-exception on
```

该命令是XuanTie DebugServer对软件断点指令恢复行为的控制：开启后，继续运行时的受控`ebreak`产生Breakpoint异常，而不是再次停在Debug Mode。这里不需要、也不应依赖手工读写`DCSR`。

该配置同时强制VS Code使用硬件断点：

```json
"hardwareBreakpoints": {
    "require": true,
    "limit": 5
}
```

E902当前可用的硬件断点资源为5个。硬件断点按PC地址比较，不执行软件`ebreak`，因此可以与异常自测共存。调试会话中同时启用的源码断点不得超过该上限；需要更多断点时，应分批启用或拆分调试会话。

普通`E902 demo | CKLink`配置会显式执行`monitor set resume-bkpt-exception off`，恢复调试器对软件断点的默认处理。这个显式关闭是必要的，因为DebugServer配置可能在同一服务进程的后续连接中保留。

## 3. 构建与运行

普通构建不启用异常自测：

```sh
make BOARD=t22-deserializer-evb APP=demo BUILD=debug \
     O=build/t22-deserializer-evb/demo/debug
```

异常自测使用独立应用构建：

```sh
make BOARD=t22-deserializer-evb APP=e902-exception-test BUILD=debug \
     O=build/t22-deserializer-evb/e902-exception-test/debug
```

测试构建不能作为普通运行固件长期保留，因为它会主动执行`ebreak`并依赖异常恢复逻辑。

### 3.1 直接下载运行

将`firmware.bin`下载到`0x00140000`后复位运行，观察早期UART输出。该方式用于验证脱离调试器时的完整异常路径。

### 3.2 使用CKLink运行

1. 在WSL中完成异常自测构建。
2. 启动XuanTie DebugServer并连接CKLink。
3. 在VS Code中选择`E902 exception test | CKLink`并启动调试。
4. 该配置会复位并暂停目标、下载`firmware.bin`、打开`resume-bkpt-exception`，随后自动继续运行。
5. 观察UART日志是否输出`PASS`。

需要在自测代码中停住时，应在启动前设置不超过5个源码断点。启动配置会将它们下发为硬件断点；调试控制台执行`info breakpoints`时应显示`hw breakpoint`。

## 4. 测试流程

### 4.1 启动和异常入口

测试程序启动后先输出正常启动信息，然后进入异常现场自测函数。自测函数在执行`ebreak`前设置寄存器特征值，并保存期望值：

```text
x1  = 0x11111111
x4  = 0x44444444
...
x15 = 0xFFFFFFFF
```

`x2/sp`和`x3/gp`不能简单覆盖为测试特征值，因为它们必须继续指向有效栈和全局数据区域。测试函数使用栈哨兵检查异常现场是否越界。

### 4.2 异常现场记录

执行`ebreak`后，硬件进入`mtvec`，汇编入口建立固定RV32E现场并调用`e902_exception_dispatch()`。C层应记录并输出：

```text
mcause
mepc
mtval
mstatus
x1-x15
```

预期`mcause`满足：

```text
Interrupt = 0
ExceptionCode = 3   // Breakpoint
```

预期`mepc`指向自测函数中的`ebreak`地址。

### 4.3 断点恢复

当前测试汇编使用：

```asm
.option push
.option norvc
ebreak
.option pop
```

因此当前实际测试的是4字节`ebreak`，恢复时应执行：

```text
mepc = mepc + 4
恢复GPR和CSR
mret
```

长度识别逻辑还必须满足：

```text
低16位 == 0x9002                       -> c.ebreak，长度2
低两位 == 2'b11 且完整编码 == 0x00100073 -> ebreak，长度4
其他编码                                -> 不恢复
```

如果后续增加`c.ebreak`测试，应单独使用未启用`.option norvc`的测试入口，并验证返回地址只前移2字节。

### 4.4 恢复后的现场检查

`mret`返回自测函数后，测试函数立即保存恢复后的寄存器值并逐项比较：

1. 比较`x1-x15`与异常前的特征值。
2. 检查`sp`仍指向当前测试栈。
3. 检查`gp`仍保持有效全局指针。
4. 检查栈哨兵没有被覆盖。
5. 检查异常计数为1且没有进入停机状态。

只有所有项目通过，才输出`PASS`。

## 5. 预期日志

日志的具体寄存器值随链接地址和运行时栈地址变化，不能把地址数值写死。正常流程应包含以下关键阶段：

```text
T22 deserializer EVB booting...
E902 exception self-test: trigger ebreak
[E902 exception]
mcause=...
mepc=...
mtval=...
mstatus=...
action=resume reason=ebreak next_mepc=...
E902 exception self-test: resumed
E902 exception self-test: PASS
```

出现`action=halt`时，说明异常分发没有批准恢复，测试程序会停在异常停机循环中。这对于普通异常是正确的默认行为，但对受控`ebreak`自测表示测试配置或断点识别没有生效。

## 6. 通过标准

- 只进入一次预期异常入口。
- `mcause`为同步Breakpoint异常。
- `mepc`指向预期的`ebreak`指令。
- `mepc`按实际指令长度前移。
- `x1-x15`、`sp`和`gp`恢复正确。
- 异常栈帧哨兵保持不变。
- `mret`返回自测函数并继续执行。
- 测试结束输出`PASS`。
- 普通构建不触发主动异常测试。

### 当前完成记录

已在T22解串器EVB上完成以下验证：

- 直接下载运行时，受控32位`ebreak`进入异常入口并输出`PASS`。
- 使用CKLink与XuanTie DebugServer时，专用启动配置能够使受控`ebreak`进入异常入口并输出`PASS`。
- CKLink异常自测配置下，源码硬件断点能够停住，且不影响受控`ebreak`的异常路径。

## 7. 失败分析

| 现象 | 优先检查项 |
| --- | --- |
| 没有进入`mtvec` | `mtvec`地址、64字节对齐、启动代码是否写入正确入口 |
| `ebreak`停在Debug Mode | CKLink异常自测配置是否执行`monitor set resume-bkpt-exception on`；直接下载时是否仍连接了会截获`ebreak`的调试器 |
| 源码断点没有停住 | 异常自测配置是否强制硬件断点、当前启用数量是否超过5个、是否在启动调试会话前设置断点 |
| 进入入口后立即再次异常 | 异常栈地址、`sp`初始化、入口保存顺序、现场帧大小 |
| `mcause`不是3 | 实际执行的不是`ebreak`，或异常来源被调试器改变 |
| `mepc`没有前移 | 测试宏未启用，或断点编码识别失败 |
| 返回后寄存器错误 | GPR保存偏移、`sp`恢复顺序、`mret`前CSR恢复顺序 |
| 栈哨兵被破坏 | 异常帧大小、栈边界、编译器调用约定和栈对齐 |
| 普通固件也触发测试 | 错误选择了`APP`、应用构建开关泄漏或构建缓存未清理 |

## 8. 测试代码对应关系

| 内容 | 文件 |
| --- | --- |
| 独立自测应用和构建开关 | `apps/e902-exception-test/app.mk` |
| 自测启动和PASS/FAIL判断 | `apps/e902-exception-test/main.c` |
| 寄存器特征值、`ebreak`和恢复后比较 | `apps/e902-exception-test/exception_self_test_gcc.S` |
| 断点长度识别和恢复策略 | `rt-thread/libcpu/risc-v/e902/exception.c` |
| 异常现场入口和`mret` | `rt-thread/libcpu/risc-v/e902/exception_gcc.S` |

## 9. 后续验证

当前异常自测完成后，继续增加测试时应保持每次只引入一个变量：

1. CLIC软件中断和`mtvt`硬件向量入口。
2. DW Timer IRQ 27和共享通道清源。
3. RV32E线程初始栈和上下文切换。
4. RT-Thread调度器和ISR退出调度。
5. 长时间运行、栈水位、Tick漂移和异常恢复测试。

非法地址访问不应作为默认自测手段。若T22总线对未映射地址不返回错误而是永久等待，CPU可能无法进入异常入口，应先确认总线超时和看门狗复位机制。
