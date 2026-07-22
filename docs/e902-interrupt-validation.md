# E902异常与CLIC验证

本文记录T22 E902同步异常入口和CLIC通用中断分发的验证方法、构建方式、实测结果和失败分析。它与[《E902异常与CLIC中断架构》](e902-interrupt-architecture.md)分工如下：

- 架构文档解释硬件机制、寄存器关系、现场边界和默认异常策略。
- 本文解释如何通过独立、受控的测试应用证明异常和中断路径实现正确。

异常和CLIC测试分别通过独立的`e902-exception-test`和`e902-clic-test`应用显式启用，普通`demo`固件不执行主动异常或CLIC pending测试。

> 2026-08-07目标板回归已通过：受控`ebreak`能够进入异常入口并恢复，IRQ 7能够经Board唯一`rt_irq_desc`表完成标准RT-Thread IRQ分发并返回；Debug/Release构建和静态检查同步通过。CKLink异常测试配置已在`launch.json`中提前设置`resume-bkpt-exception on`，避免测试`ebreak`停留在Debug Mode。

## 1. 异常验证范围

异常自测覆盖以下内容：

| 验证项 | 目标 |
| --- | --- |
| 异常入口 | `mtvec`指向E902公共异常入口，入口能够保存RV32E现场 |
| CSR现场 | 正确保存`mepc`、`mstatus`、`mcause`和`mtval` |
| GPR现场 | `x1-x15`、`sp`和`gp`在异常返回后保持正确 |
| 栈边界 | 异常栈帧不越界、不破坏哨兵 |
| 断点恢复 | 受控`ebreak`能够按指令长度修改`mepc`并执行`mret` |
| 异常隔离 | 未注册测试Hook时，异常只诊断并停机 |
| CKLink兼容性 | 调试器连接时，受控`ebreak`仍进入异常入口，源码断点仍可由硬件断点停住 |

当前自测使用`.option norvc`生成32位`ebreak`。断点指令长度识别属于独立测试Hook，不属于CPU port默认异常策略；压缩断点的独立执行用例应单独增加，不能仅凭代码阅读视为已经完成验证。

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

当前通过WSL调用Windows原生玄铁GCC时，使用相对`BUILD_DIR`；`O`仍兼容纯Linux工具链。

```sh
make BOARD=t22-deserializer-evb APP=demo BUILD=debug \
     BUILD_DIR=build/t22-deserializer-evb/demo/debug
```

异常自测使用独立应用构建：

```sh
make BOARD=t22-deserializer-evb APP=e902-exception-test BUILD=debug \
     BUILD_DIR=build/t22-deserializer-evb/e902-exception-test/debug
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

执行`ebreak`后，硬件进入`mtvec`，汇编入口在当前栈上建立固定RV32E现场并调用`e902_exception_dispatch()`。测试应用通过`rt_hw_exception_install()`注册Hook，直接从该栈帧检查以下字段，不复制全局异常快照：

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

因此当前实际测试的是4字节`ebreak`。Breakpoint编码识别和恢复策略全部位于独立测试Hook中；CPU port只负责调用Hook并根据`RT_EOK`决定是否恢复。Hook执行：

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
5. 确认受控异常恢复后测试函数继续执行，且寄存器比较结果通过。

只有所有项目通过，才输出`PASS`。

## 5. 本轮目标板实测日志

日志的具体寄存器值随链接地址和运行时栈地址变化，不能把地址数值写死。2026-08-07已在T22解串器EVB上完成复测，实测日志为：

```text
T22 deserializer EVB booting...
E902 exception self-test: trigger ebreak
E902 exception self-test: resumed
E902 exception self-test: PASS
```

如果测试Hook拒绝处理，CPU port会打印`[E902 exception]`和完整现场，随后输出`action=halt reason=unrecoverable-exception`并停机。这对于普通异常是正确的默认行为，但对受控`ebreak`自测表示Hook注册、断点原因、`mepc`或指令编码检查没有通过。

## 6. 通过标准

- 受控`ebreak`进入预期异常入口并成功恢复。
- `mcause`为同步Breakpoint异常。
- `mepc`指向预期的`ebreak`指令。
- `mepc`按实际指令长度前移。
- `x1-x15`、`sp`和`gp`恢复正确。
- 异常栈帧哨兵保持不变。
- `mret`返回自测函数并继续执行。
- 测试结束输出`PASS`。
- 普通构建不触发主动异常测试。

### 本轮目标板完成记录

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
| 断点长度识别和恢复策略 | 独立`e902-exception-test`应用的Hook |
| 异常现场入口和`mret` | `rt-thread/libcpu/risc-v/e902/cpuport_gcc.S` |

## 9. CLIC通用分发验证

### 9.1 验证范围与状态

CLIC自测用于验证从控制器初始化到`mret`返回主程序的最小异步中断闭环：

| 验证项 | 目标 |
| --- | --- |
| 硬件发现 | 读取`CLICINFO`，校验中断数量和E902支持的2至5个控制位 |
| 全局初始化 | 在`mstatus.MIE=0`时禁用全部硬件IRQ、清除历史pending并配置`CLICCFG.nlbits`和`MINTTHRESH` |
| 硬件向量表 | `mtvt`指向64字节对齐、包含80项的T22向量表 |
| IRQ注册 | IRQ 7配置为`shv=1`、正边沿触发和最低逻辑level，安装处理函数及参数 |
| 公共入口 | 保存RV32E `x1-x15`和必要CSR，通过`mcause`分发处理函数 |
| pending生命周期 | 全局中断关闭时置位并读回IP；硬件接受正边沿向量中断后观察自动清除，ISR不重复清除 |
| 返回路径 | `mcause.Interrupt=1`且中断号为7，ISR只执行一次，并通过`mret`返回主程序 |
| 测试隔离 | CLIC测试只存在于独立应用，不改变普通`demo`的运行流程 |

旧版自测曾使用IRQ 3并已在目标板输出`PASS`。IRQ 3现在由上下文切换专用入口独占，不再经过`e902_irq_dispatch()`，所以通用分发自测迁移到T22明确实现、且当前不作为系统Tick使用的Core Timer IRQ 7。本轮标准IRQ框架版本已经完成构建和目标板回归。

### 9.2 构建与运行

使用独立应用和独立输出目录构建：

```sh
make BOARD=t22-deserializer-evb APP=e902-clic-test BUILD=debug \
     BUILD_DIR=build/t22-deserializer-evb/e902-clic-test/debug
```

直接运行时，将该目录下的`firmware.bin`下载到`0x00140000`并复位，观察UART日志。

使用CKLink时，启动XuanTie DebugServer，在VS Code中选择`E902 CLIC test | CKLink`。该配置会复位并暂停目标、下载CLIC测试固件、执行以下命令后自动继续：

```gdb
monitor set resume-bkpt-exception off
```

CLIC测试不主动执行`ebreak`，因此保留调试器对软件断点的正常接管行为。该配置只负责下载和调试，不触发WSL编译。

### 9.3 测试流程

1. 启动代码保持`mstatus.MIE=0`，完成`.data`、`.bss`和板级UART初始化。
2. 链接脚本在构建期确认80项向量表大小，`rt_hw_interrupt_init()`初始化Board唯一的RT-Thread IRQ描述表，并用T22持有的CLIC实例调用E902 IRQ适配层。
3. 通用`riscv_clic`驱动读取`CLICINFO`，拒绝非法中断数量或控制位数，并禁用所有已实现IRQ、清除历史pending。
4. 通用驱动令`nlbits=CLICINTCTLBITS`，并使用T22 SoC提供的阈值地址将`MINTTHRESH.mth`初始化为0；E902适配层校验控制位范围并写入`mtvt`。
5. E902初始化确认硬件中断数量不超过向量表容量；Board提供同等容量的唯一`rt_irq_desc`表，链接期断言确认T22的80项向量表容量足够。
6. 测试应用不初始化Core Timer计数功能；它用`riscv_clic_configure_irq()`把IRQ 7配置为`shv=1`、正边沿和最低逻辑level，再通过`rt_hw_interrupt_install()`安装标准RT-Thread ISR。
7. 清除IRQ 7 pending，通过`rt_hw_interrupt_umask()`打开`CLICINTIE[7]`，此时全局`mstatus.MIE`仍为0。
8. 软件写`CLICINTIP[7]=1`并读回1，证明请求已锁存但尚未进入ISR。
9. 最后打开`mstatus.MIE`，E902通过`mtvt[7]`进入普通中断入口。
10. 公共入口保存现场，`e902_irq_dispatch()`取得`mcause`中的IRQ号，再由Board的`rt_hw_interrupt_dispatch()`查找唯一`rt_irq_desc`表并调用已安装处理函数。
11. 正边沿且`shv=1`的pending在硬件接受中断时应自动清零；ISR只记录该状态，不再执行可能清除后续新边沿的冗余写0。
12. 入口恢复`mcause`、`mstatus`、`mepc`和GPR，通过`mret`返回；主程序关闭全局中断并通过`rt_hw_interrupt_mask()`关闭IRQ 7后统一判定结果。

测试采用单层中断，ISR内不重新打开`mstatus.MIE`，本阶段不验证中断嵌套和RT-Thread中断嵌套计数。

### 9.4 本轮目标板实测日志与通过条件

2026-08-07标准IRQ框架重构后的目标板实测日志如下：

```text
T22 deserializer EVB booting...
E902 CLIC self-test: init
E902 CLIC self-test: CLICINFO=0x00600050 irq_count=0x00000050 ctlbits=0x00000003
E902 CLIC self-test: trigger IRQ 7
E902 CLIC self-test: handled IRQ 7
E902 CLIC self-test: PASS
```

实测值可解析为：

```text
CLICINFO.num_interrupt  = 0x50 = 80
CLICINFO.CLICINTCTLBITS = 3
CLICINTCTL有效位        = bit[7:5]
CLICCFG.nlbits          = 3
独立priority位          = 0
```

测试程序只有在以下条件全部满足时才会输出`PASS`：

- IRQ处理函数只执行一次，接收到的IRQ号为7。
- 公共入口根据`mcause`完成分发，注册参数保持正确。
- 打开全局中断前pending为1，进入ISR后pending已经自动变为0。
- IRQ 7处理函数只执行1次，处理函数收到的IRQ号为7，且没有进入未注册处理函数路径。
- `mret`返回主程序，测试最终输出`PASS`。

IRQ 7测试通过后，可以证明请求从pending状态经过CLIC仲裁、`mtvt[7]`普通入口、C处理函数和现场恢复完整返回主程序。IRQ 3专用调度入口由独立上下文切换测试验证，两种路径互不借用测试代码。

### 9.5 失败码与排查入口

失败日志格式为：

```text
E902 CLIC self-test: FAIL result=0x........
```

| `result` | 失败位置 | 优先检查项 |
| --- | --- | --- |
| 1 | 初始化后信息不一致 | 信息指针、IRQ数量、80项向量容量和控制位数 |
| 2 | IRQ 7硬件属性配置失败 | IRQ边界、触发类型、逻辑level和CLIC初始化状态 |
| 3 | IRQ 7 ISR安装失败 | IRQ边界、保留IRQ策略和板级默认描述表初始化 |
| 4 | 初始pending无法清零 | IRQ 7 IP寄存器访问和触发属性 |
| 5 | pending未锁存为1 | `CLICINTIP[7]`地址、8位MMIO访问、全局中断是否提前打开和IP寄存器行为 |
| 6 | 等待ISR超时 | `mstatus.MIE`、`CLICINTIE[7]`、阈值、level、`mtvt[7]`和入口地址 |
| 7 | ISR后结果不一致 | IRQ号、pending自动清除、处理次数或参数 |

### 9.6 测试代码对应关系

| 内容 | 文件 |
| --- | --- |
| 独立CLIC自测应用 | `apps/e902-clic-test/main.c` |
| CLIC寄存器访问、注册和C分发 | `drivers/interrupt/riscv_clic/riscv_clic.c` |
| E902 `mtvt`安装和`mcause`分发适配 | `rt-thread/libcpu/risc-v/e902/e902_irq.c` |
| RV32E公共中断入口和`mret` | `rt-thread/libcpu/risc-v/e902/cpuport_gcc.S` |
| T22 IRQ编号和初始化封装 | `soc/t22-serdes/t22_serdes_irq.c`、`include/t22_serdes_irq.h` |
| 80项硬件向量表 | `soc/t22-serdes/interrupt_vectors.S` |
| 向量布局和链接期断言 | `soc/t22-serdes/linker.ld` |
| CKLink启动配置 | `.vscode/launch.json` |

## 10. 后续验证

第6阶段DW Timer必要代码和独立验证应用已经通过目标板验证，Timer测试方法和结果不继续堆叠在本文中，详见[《E902 DW Timer周期中断验证》](e902-timer-validation.md)。继续增加测试时应保持每次只引入一个变量：

1. RV32E线程初始栈和上下文切换。
2. RT-Thread调度器和ISR退出调度。
3. 长时间运行、栈水位、Tick漂移和异常恢复测试。

非法地址访问不应作为默认自测手段。若T22总线对未映射地址不返回错误而是永久等待，CPU可能无法进入异常入口，应先确认总线超时和看门狗复位机制。
