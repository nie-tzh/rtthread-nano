# RT-Thread Nano E902移植进度

## 总体移植阶段

| 阶段 | 状态 | 阶段目标 |
| --- | --- | --- |
| 1. 确认目标硬件和工具链参数 | 已完成 | 确认CPU指令集、ABI、工具链和编译参数 |
| 2. 搭建最小编译系统 | 已完成 | 建立分层、可复现且不依赖IDE的构建系统 |
| 3. 最小裸机启动验证 | 已完成 | 验证入口、栈、`.data`、`.bss`和C函数运行环境 |
| 4. 板级初始化与串口输出 | 已完成 | 完成早期时钟、引脚、UART初始化和上板日志输出 |
| 5. 建立异常与CLIC中断机制 | 已完成 | 同步异常和CLIC IRQ 3软件中断均已完成上板验证 |
| 6. 实现DW Timer并验证周期中断 | 已完成 | T22 DW Timer周期时基和共享IRQ 27已经完成上板验证 |
| 7. 实现E902线程栈与上下文切换 | 已完成 | RV32E线程栈、首次启动、IRQ 3延后切换和独立双线程验证均已通过目标板验证 |
| 8. 接入RT-Thread Nano内核与系统Tick | 未开始 | 验证调度、延时、时间片和内核Tick |
| 9. 完善驱动框架与板级外设 | 未开始 | 将板级外设接入RT-Thread Device框架 |
| 10. 建立组件与应用开发框架 | 未开始 | 建立稳定的组件、应用和配置入口 |
| 11. 开展功能、异常与稳定性测试 | 未开始 | 完成功能、压力、异常和长时间运行测试 |
| 12. 完善调试支持、文档与工程交付 | 未开始 | 形成可复现的构建、调试和交付资料 |

## 1. 确认目标硬件和工具链参数

### 1.1 阶段目标

在编写启动代码前确认处理器实际实现的指令集、ABI和工具链行为，避免把通用RISC-V或其他E902配置的代码直接用于当前芯片。

### 1.2 目标硬件

- 目标板卡：T22解串器EVB。
- CPU核：玄铁E902。
- 当前RTL配置：RV32EC，不使用M扩展。
- 运行模式：Machine模式。
- 字节序：小端。
- ABI：ILP32E。

RV32E只有`x0-x15`寄存器。后续异常入口、线程栈和上下文切换代码不能访问`x16-x31`。

### 1.3 工具链

使用玄铁裸机工具链：

```text
Xuantie-900-gcc-elf-newlib-x86_64-V3.2.0-20250627
GCC 14.1.1 20240710
```

工具链前缀：

```text
riscv64-unknown-elf-
```

编译参数：

```text
-mcpu=e902 -mabi=ilp32e -mcmodel=medlow
```

当前工具链中，`-mcpu=e902`对应的目标属性为：

```text
-march=rv32ec_zicsr_zifencei_zca_xtheadcmo_xtheadse
-mabi=ilp32e
```

其中没有`m`扩展，因此不能生成硬件乘除法指令，也不能直接使用按E902M构建的目标文件。

### 1.4 确认方法

查看工具链对CPU参数的解释：

```shell
riscv64-unknown-elf-gcc -mcpu=e902 -mabi=ilp32e \
    -Q --help=target
```

查看最终ELF属性：

```shell
riscv64-unknown-elf-readelf -h -A firmware.elf
```

当前ELF已经确认：

- 文件类型为`ELF32`。
- Machine为RISC-V。
- ELF Flags包含RVE和RVC。
- ABI为soft-float ILP32E。
- `Tag_RISCV_arch`与工具链展开结果一致。

## 2. 搭建最小编译系统

### 2.1 阶段目标

建立不依赖IDE工程文件的最小Makefile构建系统，使CPU、SoC、Board、驱动和应用具有明确边界，并保证编译参数和产物可追踪。

### 2.2 构建配置

默认构建身份：

```text
BOARD=t22-deserializer-evb
  -> SOC=t22-serdes
  -> CHIP=t22-deserializer

SOC=t22-serdes
  -> CPU=e902
```

构建系统校验Board、SoC、CHIP和CPU之间的绑定关系，不允许通过命令行组合不匹配的配置。

### 2.3 工程分层

```text
Makefile                         统一构建入口
mk/                              工具链和通用构建规则
apps/                            示例和应用
boards/t22-deserializer-evb/     板级资源选择和初始化
soc/t22-serdes/                  启动、链接和SoC系统集成
drivers/                         可复用控制器驱动
rt-thread/                       RT-Thread Nano源码和CPU port
build/                           独立构建产物，不提交Git
```

根Makefile负责解析配置和加载各模块的`.mk`文件。各模块声明自己的源码、头文件和依赖，不使用递归Make。

### 2.4 编译和链接规则

当前构建系统支持：

- `.c`、`.S`和`.s`文件编译。
- `.d`头文件依赖和增量构建。
- Debug和Release配置。
- `-ffunction-sections`、`-fdata-sections`和链接垃圾回收。
- 独立链接脚本和MAP文件。
- ELF、BIN、反汇编和尺寸信息生成。
- APP、BOARD和BUILD之间的产物隔离。
- 构建参数变化后自动重编译相关对象。

### 2.5 常用命令

```shell
make
make BUILD=release
make info
make V=1
make clean
```

默认输出目录：

```text
build/t22-deserializer-evb/demo/debug/
```

主要产物：

```text
firmware.elf
firmware.bin
firmware.map
firmware.lst
```

### 2.6 验证结果

- V3.2.0工具链可以完成全量构建。
- 编译和链接无警告、无未定义符号。
- 独立目录重复构建得到相同的`firmware.bin`。
- `build/`已经由Git忽略。
- 工具链通过`PATH`查找，工程不记录本机绝对路径。

## 3. 最小裸机启动验证

### 3.1 阶段目标

在引入RT-Thread和中断前，先确认CPU可以从预期地址执行，并建立正确的C语言运行环境。

### 3.2 内存布局

| 区域 | 起始地址 | 长度 | 用途 |
| --- | --- | --- | --- |
| ROM | `0x00140000` | `0x00010000` | XIP代码、只读数据和`.data`加载镜像 |
| RAM | `0x00150000` | `0x0000F000` | `.data`、`.bss`、堆和启动栈 |
| CONFIG | `0x0015F000` | `0x00001000` | 独立配置区域 |

启动栈配置：

```text
__stack_top   = 0x0015F000
STACK_SIZE    = 0x1000
__stack_limit = 0x0015E000
```

链接脚本使用断言检查ROM溢出、RAM与栈重叠以及RAM/CONFIG边界。

### 3.3 启动流程

`_start`当前执行顺序：

```text
初始化gp
    -> 初始化sp
    -> 从ROM复制.data到RAM
    -> 清零.bss
    -> 调用board_init()
    -> 调用main()
    -> main返回后停留在死循环
```

最小裸机阶段最初只要求进入`main()`循环；`board_init()`是在第4阶段加入的。

### 3.4 调试标记

应用使用初始化变量和零初始化变量辅助确认`.data`复制及`.bss`清零：

```c
volatile uint32_t g_boot_marker = 0x45563930U;
volatile uint32_t g_runtime_marker;
```

进入`main()`后将`g_boot_marker`写入`g_runtime_marker`，可以同时验证两个数据区。

普通C变量的地址会随源码和链接顺序变化。调试时应通过ELF符号或MAP文件定位，不再把`0x150000`、`0x150004`等地址写成固定接口。

### 3.5 验证方法

1. 将`firmware.bin`写入`0x00140000`。
2. 复位并暂停CPU，确认PC从`0x00140000`开始执行。
3. 在`_start`、`board_init`和`main`设置断点。
4. 确认`gp`和`sp`初始化正确。
5. 确认`.data`内容与ROM加载镜像一致。
6. 确认`.bss`在进入C代码前为0。
7. 确认程序最终进入`main()`且没有异常复位。

### 3.6 验证结果

- ELF入口为`0x00140000`。
- `_start`、`.data`复制和`.bss`清零均已执行。
- 栈顶为`0x0015F000`。
- 程序可以进入`main()`并持续运行。

## 4. 板级初始化与串口输出

### 4.1 阶段目标

在最小启动基础上建立SoC和Board初始化流程，通过不依赖`printf`和libc的轮询UART输出第一条可见日志。

### 4.2 初始化分层

```text
startup.S
    -> board_init()
        -> t22_serdes_soc_early_init()
        -> UART2 TX引脚配置
        -> UART2复位
        -> DW APB UART初始化
    -> main()
        -> board_early_puts()
```

各层职责：

- SoC层负责时钟、CSR受保护写、复位和T22芯片集成操作。
- Board层选择UART2作为当前EVB早期控制台，并提供波特率配置。
- DW APB UART驱动负责通用寄存器操作和轮询发送。
- App只调用Board提供的早期输出接口。

### 4.3 T22早期初始化

当前SoC初始化完成：

- 打开System Clock 2。
- 配置AHB为320 MHz。
- 配置APB为200 MHz。
- 配置SPI为200 MHz。
- 通过T22 CSRAO写控制寄存器执行受保护CSR写。
- 配置解串器MFP14为UART2 TX。
- 根据eFuse判断旧版芯片是否需要TX-enable ECO反相处理。
- 先拉低再释放UART2复位。

### 4.4 UART参数

| 参数 | 当前配置 |
| --- | --- |
| UART实例 | UART2 |
| 基地址 | `0x00102000` |
| 输入时钟 | 200 MHz APB |
| 波特率 | 115200 |
| 数据格式 | 8N1 |
| 发送方式 | 查询LSR THRE位 |
| 换行处理 | `\n`转换为`\r\n` |

驱动对波特率除数进行四舍五入，并为寄存器busy和发送等待设置超时，避免硬件异常时永久卡死在初始化或输出函数中。

### 4.5 上板验证

程序进入`main()`后输出：

```text
T22 deserializer EVB booting...
```

当前已经完成以下验证：

- 板级代码和UART驱动参与最终链接。
- ELF入口和内存布局保持不变。
- UART时钟、引脚、复位和波特率配置可以驱动实际硬件输出。
- 输出过程不依赖RT-Thread、`printf`、动态内存或libc初始化。

## 5. 建立异常与CLIC中断机制

### 5.1 阶段目标

在接入系统Tick和线程调度前，分别验证同步异常入口和CLIC异步中断路径。该阶段拆成两个可独立验证的实现步骤。

### 5.2 E902异常入口、现场诊断与验证（已完成）

已建立E902同步异常公共入口、RV32E现场帧和默认“诊断后停机”策略。异常自测通过独立的`e902-exception-test`应用启用，不会进入普通`demo`固件。

已完成的验证范围：

- `mtvec`以CLIC模式安装，并满足64字节对齐要求。
- 异常入口只使用RV32E存在的`x0-x15`寄存器。
- 能保存和恢复必要的GPR和CSR现场，并通过`mret`返回。
- 受控32位`ebreak`触发后，可以通过早期串口观察`mcause`、`mepc`、`mtval`和恢复动作。
- 自测检查`x1-x15`、`sp`、`gp`和栈哨兵，结束输出`PASS`。
- 已完成直接下载和CKLink两种运行方式验证；CKLink方式使用硬件断点，避免与自测`ebreak`冲突。
- 正常固件不包含主动异常测试。

详细构建、运行、预期日志和失败分析见[《E902异常与CLIC验证》](e902-interrupt-validation.md)。

### 5.3 T22 CLIC初始化与软件中断验证（已完成）

当前实现包括：

- E902 CPU层读取并校验`CLICINFO`，禁用全部硬件IRQ，配置并回读`mtvt`、`CLICCFG.nlbits`和`MINTTHRESH`。
- 建立统一的RV32E中断现场入口，保存`x1-x15`及必要CSR，通过`mcause`分发已注册处理函数并执行`mret`。
- T22 SoC层集中定义IRQ号，提供64字节对齐、80项的`mtvt`硬件向量表和描述符表。
- 链接脚本保留向量段，并检查向量表对齐和精确大小。
- 通过独立`e902-clic-test`应用配置IRQ 3为`shv=1`、正边沿触发，在最后一步打开`mstatus.MIE`。
- 提供直接下载和`E902 CLIC test | CKLink`两种运行入口；CKLink配置只下载并调试，不执行编译。

已完成的静态检查：

- Debug和Release CLIC测试固件均能使用E902工具链无警告编译、链接。
- 普通`demo`和异常自测固件完成回归构建。
- ELF为RV32E、ILP32E，入口仍为`0x00140000`。
- `.vectors`为64字节对齐的320字节段，80项均指向公共中断入口。
- 反汇编确认入口只访问`x0-x15`，按统一80字节现场保存和恢复CSR/GPR。

上板验证结果：

- 实测`CLICINFO=0x00600050`，总IRQ数为80，`CLICINTCTLBITS=3`。
- 全局中断打开前IRQ 3 pending成功锁存为1。
- IRQ 3经`mtvt[3]`进入公共入口，`mcause.Interrupt=1`且中断号为3。
- 正边沿硬件向量中断被接受后pending自动清零，处理函数只执行一次。
- `mret`正确返回主程序，串口输出`E902 CLIC self-test: PASS`。

具体构建命令、实测日志和失败码见[《E902异常与CLIC验证》](e902-interrupt-validation.md)。第5阶段已经完成；第6阶段的DW Timer必要代码和独立验证应用也已通过目标板验证。

## 6. 实现DW Timer并验证周期中断

### 6.1 阶段目标

先建立与RT-Thread内核无关的稳定周期时基，验证T22 DW Timer、CLIC IRQ 27和公共中断入口能够持续协同工作。第6阶段只通过回调报告周期事件；`rt_tick_increase()`以及RT-Thread中断进入、退出边界在第8阶段接入。

### 6.2 已确定的硬件资源

| 项目 | 当前配置 |
| --- | --- |
| 控制器基地址 | `0x00103400` |
| 通道数量 | 8 |
| 通道步长 | `0x14` |
| 公共状态寄存器 | `0x001034A0`，低8位对应8个通道 |
| 共享中断 | CLIC IRQ 27，高电平触发、硬件向量模式 |
| 输入时钟 | 200 MHz APB |
| Tick通道 | 零基通道0，即TIMER1，基地址`0x00103400` |
| 计数模式 | 周期模式，`TxControl.MODE=1` |
| 清中断 | 读取对应通道`TxEOI` |

通道0的选择与当前T22产品参考配置`CONFIG_SYS_TICK_DW_TIMER=0`一致。该通道由Board层保留为系统Tick资源，其他模块不能绕过T22 Timer接口直接配置它。

周期装载值按下式计算：

```text
load_count = APB_CLOCK_HZ / frequency_hz
```

当前接口要求频率能够整除200 MHz，否则返回`BOARD_TICK_ERROR_FREQUENCY`，避免静默引入累计频率误差。后续按RT-Thread默认`1000 Hz`运行时，`load_count=200000`。

### 6.3 软件分层

```text
Board: board_tick_init/start/stop
    -> 固定选择TIMER1，保存系统Tick回调
SoC: t22_serdes_timer_*
    -> 管理8个通道、活动掩码和共享IRQ 27分发
Driver: dw_apb_timer_*
    -> 寄存器布局、装载、周期模式、启停、状态和EOI
CPU: e902_clic_*
    -> CLIC属性、使能和公共中断现场
```

通用驱动不知道T22基地址、IRQ号或系统Tick通道；Board层不直接访问Timer MMIO；CPU port不包含DW Timer寄存器。寄存器布局和通道步长使用编译期断言检查。

### 6.4 共享中断处理

IRQ 27处理函数先读取公共状态寄存器快照，再遍历所有置位通道。每个通道必须先读取自身`TxEOI`撤销外设中断源，随后才调用已注册回调，不能在处理第一个pending通道后提前返回。

活动掩码用于管理共享中断线：

- 第一个通道启动时使能`CLICINTIE[27]`。
- 停止某个通道时只清除该通道活动位。
- 最后一个通道停止后关闭`CLICINTIE[27]`。
- 若出现没有有效回调的pending通道，处理函数会清源并停止该通道，避免形成中断风暴。

初始化和启动顺序为：

```text
t22_serdes_irq_init()
    -> board_tick_init(frequency_hz, handler, parameter)
    -> board_tick_start()
    -> e902_global_irq_enable()
```

CLIC必须先于Timer注册完成，全局中断必须在入口、回调和清源路径全部就绪后最后打开。

### 6.5 验证状态

必要代码已经完成以下静态验证：

- `demo`、异常测试和CLIC测试应用均可无警告回归编译、链接。
- 独立`e902-timer-test`应用的Debug和Release构建均已通过。
- 使用`-Wconversion`、`-Wshadow`、`-Wcast-qual`等更严格的告警选项检查Timer测试代码。
- 强制保留全部Timer公开接口完成链接，确认没有被链接垃圾回收掩盖的未定义符号。
- ELF保持RV32E、RVC和ILP32E属性，入口及内存布局不变。

独立`e902-timer-test`应用已经完成目标板验证，验证方法、日志格式和失败码见[《E902 DW Timer周期中断验证》](e902-timer-validation.md)。目标板最终输出`E902 DW Timer self-test: PASS`，确认：

- 以独立`mcycle`测量TIMER1连续100个周期，实测结果处于2%允许误差内。
- 两个Timer通道通过共享IRQ 27均能得到分发和清源。
- 停止TIMER1后TIMER2继续运行，TIMER1计数和回调保持不变。
- TIMER1能够再次启动，两个通道最终停止后IRQ 27 pending为0。

第6阶段已经完成，下一步进入第7阶段，实现E902线程初始栈和上下文切换。当前约130 ms的功能自测不替代第11阶段的长时间Tick漂移、丢Tick和稳定性测试。

## 7. 实现E902线程栈与上下文切换

### 7.1 阶段目标

在尚未接入RT-Thread调度器和系统Tick前，先建立与当前Nano内核接口兼容的RV32E线程现场，完成首次线程启动和IRQ 3延后上下文切换。第7阶段只验证CPU port保存、选择和恢复线程现场的能力；线程就绪队列、调度决策和`rt_tick_increase()`在第8阶段接入。

### 7.2 线程现场

当前线程现场与已验证的异常/中断入口共用80字节布局：

```text
x1-x15 + mepc + mstatus + mcause + mtval + reserved
```

玄铁SDK的E902 RT-Thread参考port使用17个32位槽，只包含`x1-x15 + mepc + mstatus`。本工程保留统一的20槽现场，是因为IRQ 3继续经过现有CLIC公共入口和C分发。普通IRQ返回同一现场时恢复其中的`mcause`；IRQ 3切换线程时，`mcause`属于当前正在退出的IRQ 3，切换`sp`后必须保留当前CSR中的值，不能从目标线程现场恢复。`mtval`和`reserved`不参与普通线程调度，但保留后可与现有入口共享同一现场布局。

`rt_hw_stack_init()`按照RT-Thread传入的栈顶地址向下构造初始现场，并设置：

- `ra = thread_exit`。
- `sp = 对齐后的线程栈顶`。
- `gp = __global_pointer$`。
- `a0 = parameter`。
- `mepc = thread_entry`。
- `mstatus.MPP = M`、`mstatus.MPIE = 1`。
- 初始`mcause = 0`，由`rt_hw_context_switch_to()`在第一次启动线程时装载；IRQ 3切换线程时不装载该字段。

### 7.3 切换请求与实际切换

当前Nano内核向CPU port传入的是线程控制块中`sp`字段的地址，即`&thread->sp`。普通线程调度和中断态调度分别调用`rt_hw_context_switch()`与`rt_hw_context_switch_interrupt()`，两者在E902第一版实现中使用同一延后切换路径：

```text
记录第一个from字段地址
    -> 始终更新最终to字段地址
    -> 置位切换标志
    -> 设置CLICINTIP[3]
    -> IRQ 3公共入口保存当前80字节现场
    -> current from->sp = 当前现场地址
    -> sp = target to->sp
    -> 保留当前IRQ 3的mcause
    -> 恢复目标线程的mstatus、mepc和GPR并mret
```

IRQ 3尚未处理时出现新的调度请求，只更新最终`to`，不覆盖第一次记录的真实`from`。若请求发生在硬件ISR中，IRQ 3在当前`mstatus.MIE=0`期间保持pending，待硬件ISR完成`mret`后再执行实际线程切换。

第一个线程不经过IRQ 3。`rt_hw_context_switch_to()`直接读取`to->sp`，装载人工构造的初始现场并通过`mret`进入线程入口。

### 7.4 当前状态

必要代码已经完成以下静态验证：

- `demo`、异常、CLIC和DW Timer测试应用均可无警告回归构建。
- Debug和Release工具链均能汇编新增上下文入口。
- 使用额外严格告警检查上下文C代码。
- 反汇编确认新增代码只访问RV32E存在的`x0-x15`。
- ELF继续保持RV32E、RVC和ILP32E属性。

独立`e902-context-switch-test`应用已经加入，验证方法、理论计数、实测日志和失败码见[《E902线程上下文切换验证》](e902-context-switch-validation.md)。该应用已经完成Debug、Release构建、严格告警、ELF和反汇编检查，并已通过目标板验证：20次调度请求合并为19次IRQ 3和19次实际切换，线程A/B寄存器检查通过，最终IRQ 3 pending为0，输出`E902 context self-test: PASS`。实测同时确认，IRQ 3切换线程时必须保留当前`mcause.MPIL`，不能从目标线程现场恢复`mcause`。

第7阶段已完成。下一步进入第8阶段：把CPU port接入RT-Thread调度器，建立真实线程控制块、就绪队列和调度路径，并将T22 DW Timer周期回调接入`rt_tick_increase()`。

## 设计边界和已知风险

- RT-Thread官方通用RISC-V上下文代码使用`x16-x31`，不能直接用于RV32E。
- 调试变量地址由链接结果决定。需要固定地址时应使用专用section，并由链接脚本显式放置。
- RT-Thread系统Tick使用T22 DW Timer，不使用E902 Core Timer。
- T22 RX的DW Timer基地址为`0x00103400`，包含8个通道，通道步长为`0x14`，共用CLIC IRQ 27。
- 系统Tick固定使用零基通道0（TIMER1）和当前200 MHz APB时钟；后续时钟方案变化时必须同步重新计算装载值。
- 所有DW Timer使用者必须经过共享分发层注册，不能各自覆盖IRQ 27处理函数。
- 调度器负责选择下一个线程，E902软件中断入口负责真正保存和恢复上下文。
- CLIC核心机制、T22中断号和外设中断源清除属于不同层次，实现时不能混为一个模块。
