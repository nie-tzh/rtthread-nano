# E902异常与CLIC中断架构

本文系统说明T22解串器中玄铁E902的异常与CLIC中断子系统，覆盖事件产生、CLIC仲裁、入口选择、硬件状态更新、软件现场保护、异常返回，以及中断与RT-Thread调度和上下文切换之间的关系。文中同时结合T22裸机实现和当前RT-Thread代码解释寄存器配置与软件行为。

文中使用以下标记区分信息性质：

- **硬件事实**：来自《玄铁E902 R3S0用户手册》Rev.10。
- **T22现状**：来自T22 RX `revD`裸机工程。
- **当前方案**：本工程RT-Thread Nano实现选择的方案；尚未实现的内容会明确写为“计划”。
- **参考实现**：来自玄铁RTOS SDK中的RT-Thread `e902mt` port，只作为设计参考，不能未经审查直接复制。

## 阅读指南

建议按以下顺序阅读：

1. 先读“基础模型”，分清特权模式、CLIC入口模式和每路中断属性。
2. 再读“E902硬件机制”，理解CSR、向量选择、仲裁和pending清除。
3. 然后读“软件入口与现场”，回答“硬件保存什么、软件保存什么”。
4. 最后结合“T22与RT-Thread实现方案”和“验证目标”，理解这些机制如何落实到当前工程。

目录：

- [第一部分 基础模型](#第一部分-基础模型)
- [第二部分 E902硬件机制](#第二部分-e902硬件机制)
- [第三部分 软件入口与现场](#第三部分-软件入口与现场)
- [第四部分 T22与RT-Thread实现方案](#第四部分-t22与rt-thread实现方案)
- [第五部分 验证目标](#第五部分-验证目标)
- [附录](#附录)

开始阅读前先记住六点：

1. E902当前程序运行在M模式；C代码、汇编代码与M/U模式没有必然对应关系。
2. E902硬件固定采用CLIC入口机制，`mtvec.MODE`固定为3，软件不能切换成Direct或标准Vectored。
3. `shv`是每路中断的软件配置项；只有`shv=1`的中断才通过`mtvt`查表。
4. 硬件会更新`mepc`、`mcause`、`mstatus`等CSR，但不会自动把`x1-x15`压入内存栈。
5. 调度器只决定切换到哪个线程；寄存器保存、`sp`切换和恢复由E902 CPU port完成。
6. 当前CPU是RV32E，只有`x0-x15`；包含`x16-x31`的通用RISC-V上下文代码不能直接使用。

## 第一部分 基础模型

### 1. 中断架构的五个配置维度

E902中断机制容易混淆，是因为下面五个维度同时存在，但解决的是不同问题：

| 维度 | 决定什么 | 何时确定 |
| --- | --- | --- |
| RTL综合配置 | 指令集扩展、CLIC中断数量、有效控制位数、PMP数量等 | CPU核集成时固化 |
| CPU特权模式 | 当前代码能访问哪些CSR和受保护资源 | 复位及运行时切换 |
| 陷阱入口框架 | 异常和中断采用哪套入口规则 | E902中固定为CLIC |
| 每路入口选择 | 某个IRQ走`mtvec`公共入口还是`mtvt`向量入口 | 初始化时配置`shv` |
| 触发与仲裁 | 电平/边沿、极性、level、priority和阈值 | 初始化及运行时配置 |

它们之间的关系可以概括为：

```text
RTL综合参数
    -> 决定CLIC有多少路中断、多少个有效控制位

CPU特权模式
    -> 决定软件是否有权配置Machine CSR和CLIC

E902固定CLIC入口框架
    -> 异常使用mtvec；中断再由shv选择mtvec或mtvt

trig + CLICINTCTL + MINTTHRESH
    -> 决定请求如何产生、何时被接受、能否抢占

汇编入口 / 编译器ISR / 外设驱动 / RT-Thread
    -> 分工完成现场保护、源清除、调度和上下文切换
```

#### 1.1 RTL综合配置

以下能力在E902 RTL综合时固化，运行中的软件只能读取，不能增加或改变：

| 硬件能力 | E902可配置范围 | 当前T22结论 |
| --- | --- | --- |
| 基础指令集 | RV32E[M]C | RV32EC，无M扩展 |
| CLIC外部输入 | 1-240路 | 64路外部输入，CLIC总IRQ数为80 |
| `CLICINTCTL`有效位 | 2-5位 | 3位，即`CLICINTCTL[7:5]` |
| 可编码中断level | 最多32级 | 当前`nlbits=3`，形成8档可配置level编码 |
| PMP区域 | 0/4/8/12/16 | 当前T22方案不使用U模式隔离 |

本文引用的T22 RX `revD`裸机工程与当前RT-Thread目标运行在同一T22 E902平台上，因此其中的CLIC基地址、T22 IRQ映射和外设连接关系可以作为当前实现的直接依据，不属于“另一颗E902 SoC”的间接参考。仍需区分两类信息：E902核允许由RTL选择的参数应通过`CLICINFO`读取确认；T22固定的IRQ编号和外设连接则以T22 SoC定义为准。这样既能复用同平台裸机结论，也不会把E902核的可配置范围误写成所有芯片都相同的固定值。

当前T22目标板实测`CLICINFO=0x00600050`：bit[12:0]为`0x50`，表示80个总IRQ；bit[24:21]为3，表示每个`CLICINTCTL`寄存器实现3个高位控制位。总IRQ 0-15为核内中断，因此T22还提供IRQ 16-79共64路外部输入。

#### 1.2 M模式和U模式

E902支持Machine模式和User模式：

```text
M模式：内核、异常、中断、CLIC、驱动和调度
U模式：受PMP和系统调用约束的普通应用
```

特权模式由CPU状态决定，与代码使用C语言还是内嵌汇编无关。C和汇编都可以运行在M模式或U模式。

**T22现状**：程序从复位开始一直运行在M模式，没有通过设置`mstatus.MPP=U`并执行`mret`进入U模式。

如果未来引入U模式，典型分工是：

1. M模式配置PMP、线程上下文、异常入口和系统调用。
2. M模式设置用户线程的`mepc`、`mstatus.MPP=U`后执行`mret`。
3. U模式通过`ecall`、异常或中断进入M模式。
4. M模式完成受保护操作，再通过`mret`返回U模式。

当前第一版RT-Thread实现不引入U模式，以免同时扩大中断、上下文切换和内存隔离的调试范围。

### 2. 异常、中断和NMI

RISC-V使用Trap统称同步异常和异步中断：

| 类型 | 产生方式 | `mcause.Interrupt` | 典型例子 |
| --- | --- | --- | --- |
| Exception | 与当前指令同步 | 0 | 非法指令、访问错误、`ebreak`、`ecall` |
| Interrupt | 与当前指令流异步 | 1 | 软件中断、Timer、UART、GPIO |
| NMI | 不可屏蔽外部事件 | 0，异常号24 | 最高优先级故障事件 |

#### 2.1 E902同步异常

常用异常号如下：

| 异常号 | 类型 |
| --- | --- |
| 1 | 取指访问错误 |
| 2 | 非法指令 |
| 3 | 调试断点 |
| 4 | Load地址未对齐 |
| 5 | Load访问错误 |
| 6 | Store地址未对齐 |
| 7 | Store访问错误 |
| 8 | U模式`ecall` |
| 11 | M模式`ecall` |
| 24 | NMI |

异常发生时，`mepc`记录相关指令地址，`mtval`可能记录故障地址、非法指令编码或0。软件必须按异常类型决定重试、修复、跳过、终止还是复位。

不能对所有异常统一执行`mepc += 4`：

- E902支持16位压缩指令，指令长度可能是2字节。
- 访问错误和非法指令未必适合跳过后继续运行。
- 某些异常若不消除根因，返回后会再次触发。

当前RT-Thread异常处理默认在记录现场后停机，不修改`mepc`，也不对任何异常无条件跳过故障指令。只有经过明确设计和验证的异常策略，才允许恢复执行。

断点恢复若被纳入验证范围，必须先读取`mepc`处的16位半字：`c.ebreak`应前移2字节，32位`ebreak`应前移4字节，其他编码不应恢复。具体测试方法和预期结果见独立验证文档。

#### 2.2 E902异步中断

E902中断向量号包括：

| IRQ | 类型 |
| --- | --- |
| 3 | Machine Software Interrupt |
| 7 | Machine Timer Interrupt |
| 11 | Machine External Interrupt |
| 16-255 | CLIC外部中断0-239 |

T22把UART、I2C、SPI、GPIO、DW Timer和SerDes相关中断接到16及以上的CLIC外部中断号。

#### 2.3 NMI与lockup

E902把NMI作为异常号24报告。NMI不受`mstatus.MIE`屏蔽，因此即使普通可屏蔽中断关闭，NMI仍可成为严重故障和CPU锁定状态下的最后接管路径。

这里的lockup不是普通的“软件死循环”，而是CPU检测到陷阱处理过程本身再次发生无法安全处理的异常后进入的硬件锁定状态。典型过程是：

```text
普通代码发生同步异常
    -> CPU进入异常处理程序
    -> 异常处理程序尚未退出，又发生同步异常
    -> 原有返回状态面临再次覆盖，CPU进入lockup
```

进入lockup后，正常指令流和普通可屏蔽中断不能继续推进，不能期待普通ISR或线程修复现场。玄铁RTOS SDK的`bare_core_lockup`示例正是通过在异常回调中再次访问非法地址触发lockup，然后由定时器产生的NMI接管并留下诊断。因此lockup并不等同于立即掉电：NMI、调试接口或系统复位仍可作为检测和恢复手段，具体恢复策略由SoC集成决定。

NMI处理程序自身也必须极小且可证明不会异常。手册中“NMI处理程序内再次触发异常可能进入lockup”可以这样理解：CPU已经处于最高级别的陷阱处理路径，此时同步异常还要再次占用陷阱返回状态，硬件无法保证原NMI现场仍可安全返回，因此转入lockup，避免继续执行已经不可信的控制流。它不是说执行NMI处理函数本身就会锁定，而是NMI处理期间发生了第二个无法安全承接的异常。NMI中不进行调度，不调用复杂服务，不复用允许阻塞或可能失败的普通IRQ处理流程。

## 第二部分 E902硬件机制

### 3. CLINT、CLIC和关键寄存器

E902实现了兼容RISC-V的核心本地中断资源和CLIC。CLIC兼容CLIC SPEC 0.8，负责采样中断源、使能控制、优先级仲裁和入口分发。

#### 3.1 硬件固定能力与软件配置

**硬件事实**：

- E902固定配置CLIC，`mtvec.MODE`硬件固定为3。
- E902只在M模式响应CLIC中断，`CLICINTATTR.mode`固定为`2'b11`。
- CLIC最多表示256个中断号，其中16-255可对应最多240路外部输入。
- `CLICCFG.nvbits=1`，支持Selective Hardware Vectoring。
- `CLICINTCTLBITS`由RTL配置，E902支持2-5个有效位。

软件仍然需要配置：

- `mtvec.BASE`和`mtvt.BASE`。
- 每路`CLICINTIE`、`CLICINTATTR.shv`、`trig`和`CLICINTCTL`。
- 全局`CLICCFG.nlbits`和`MINTTHRESH`。
- `mstatus.MIE`。

#### 3.2 关键CSR

| CSR | 地址 | 作用 |
| --- | --- | --- |
| `mstatus` | `0x300` | 全局中断状态和特权返回状态 |
| `mie` | `0x304` | 标准Machine中断使能CSR |
| `mtvec` | `0x305` | 公共陷阱入口基址和MODE |
| `mtvt` | `0x307` | CLIC硬件向量表基址 |
| `mscratch` | `0x340` | 入口可使用的临时指针或栈信息 |
| `mepc` | `0x341` | `mret`返回地址 |
| `mcause` | `0x342` | 异常/中断号和CLIC扩展状态 |
| `mtval` | `0x343` | 异常附加信息 |
| `mnxti` | `0x345` | CLIC非向量中断查询、claim和咬尾辅助 |
| `mintstatus` | `0x346` | 当前CLIC中断level，即MIL |
| `mscratchcsw` | `0x348` | 按特权状态辅助交换`mscratch` |
| `mscratchcswl` | `0x349` | 按中断level辅助交换`mscratch` |
| `mclicbase` | `0x350` | CLIC基址，E902固定为`0xE0800000` |

“MIE”在RISC-V资料中容易指代两个不同对象，阅读代码时必须写出完整名称：

| 对象 | 位置 | 控制范围 |
| --- | --- | --- |
| `mstatus.MIE` | `mstatus` bit 3 | M模式可屏蔽中断的全局开关 |
| `mie` CSR | CSR `0x304` | 标准Machine Software/Timer/External等中断类别的使能位 |
| `CLICINTIE[i]` | CLIC每路8位寄存器 | CLIC IRQ `i`的单路使能 |

当前T22裸机代码没有调用`__set_MIE()`，也没有直接写`mie` CSR。`csi_vic_enable_irq(i)`写的是`CLICINTIE[i]`，所有入口和外设均准备完成后，`main_init()`末尾的`__enable_irq()`才执行`csrs mstatus, 8`，打开`mstatus.MIE`。因此当前T22 CLIC路径实际使用的是：

```text
CLICINTIE[i]：控制单个IRQ
mstatus.MIE ：控制全局是否接受可屏蔽中断
mie CSR      ：当前参考实现没有配置
```

当前RT-Thread移植已经实现CLIC基础层。`startup.S`先执行`csrci mstatus, 8`；CLIC初始化在全局中断关闭期间禁用全部单路IE，注册处理函数和配置中断属性后才能打开目标`CLICINTIE[i]`，所有入口就绪后才允许设置`mstatus.MIE`。当前实现与T22参考代码一致，不配置`mie` CSR，因此不能把`mie.MSIE`、`CLICINTIE[i]`和`mstatus.MIE`混成同一个寄存器操作。

CLIC模式下`mcause`还包含进入和返回所需的扩展状态：

| 位 | 字段 | 含义 |
| --- | --- | --- |
| 31 | `Interrupt` | 1表示中断，0表示异常 |
| 30 | `MINHV` | 硬件向量入口地址获取过程指示 |
| 29:28 | `MPP` | 进入陷阱前的特权状态镜像 |
| 27 | `MPIE` | 进入陷阱前的`mstatus.MIE`镜像 |
| 23:16 | `MPIL` | 进入当前中断前的`MINTSTATUS.MIL` |
| 11:0 | `ExceptionCode` | 异常号或IRQ号 |

`MINHV`只描述硬件取向量入口的过程，成功取得入口后会清零，不应把它当作ISR内长期有效的“本次一定是向量中断”标志。`MPIL`由`mret`恢复到`MINTSTATUS.MIL`。

#### 3.3 CLIC内存映射

E902的CLIC基地址为：

```text
CLIC_BASE = 0xE0800000
```

全局寄存器：

| 地址 | 寄存器 | 作用 |
| --- | --- | --- |
| `0xE0800000` | `CLICCFG` | level/priority位划分和硬件向量能力 |
| `0xE0800004` | `CLICINFO` | 中断数量、版本和有效控制位数 |
| `0xE0800008` | `MINTTHRESH` | M模式中断阈值 |

这三个全局寄存器使用字对齐地址访问。`CLICCFG`的有效配置位在低8位；`MINTTHRESH.mth`位于bit[31:24]，写阈值时不能误写到低8位。每路IP、IE、ATTR和CTL则是连续排列的8位寄存器。

`CLICINFO.CLICINTCTLBITS`不是一个布尔bit，而是`CLICINFO`中的多位字段，T22 CSI头文件把它定义在bit[24:21]。字段值`N`表示每个8位`CLICINTCTL`寄存器中真正由硬件实现的高`N`位数量。E902允许RTL选择2到5位：

```text
假设CLICINTCTLBITS = 3

CLICINTCTL[7:5]：软件可配置、参与level/priority编码
CLICINTCTL[4:0]：未实现，按1补齐后参与有效编码比较
可配置编码数量：2^3 = 8
```

`CLICCFG.nlbits`再从这`N`个有效控制位中划分多少位作为level，剩余有效位作为同level内的priority。T22参考代码和当前RT-Thread实现都令`nlbits = CLICINTCTLBITS`，所以所有已实现位都作为level使用，不再保留独立priority位。目标板实测`CLICINTCTLBITS=3`，因此当前使用3个level位和0个独立priority位。

`MINTTHRESH`是M模式的全局接收门槛，不是当前正在执行中断的level。某路IRQ即使已经pending且`CLICINTIE[i]=1`，其有效level编码也必须严格高于`MINTTHRESH.mth`才有资格被CPU接受。提高阈值可以成批屏蔽低level请求，但不会清除pending，也不会修改各路IE和CTL。

当前T22裸机`clic_config()`没有调用`csi_vic_set_thresh()`，也没有直接写`MINTTHRESH`，因此源码本身不能证明运行值；它依赖复位状态或前级启动环境。当前RT-Thread CLIC初始化在`mstatus.MIE=0`期间显式写`MINTTHRESH.mth=0x00`并回读校验，再通过各路`CLICINTIE`控制启用范围。

IRQ `i`对应四个8位寄存器：

| 地址 | 寄存器 | 作用 |
| --- | --- | --- |
| `CLIC_BASE + 0x1000 + 4*i + 0` | `CLICINTIP[i]` | pending状态 |
| `CLIC_BASE + 0x1000 + 4*i + 1` | `CLICINTIE[i]` | 单路使能 |
| `CLIC_BASE + 0x1000 + 4*i + 2` | `CLICINTATTR[i]` | `mode`、`trig`和`shv` |
| `CLIC_BASE + 0x1000 + 4*i + 3` | `CLICINTCTL[i]` | level和priority编码 |

每路IP、IE、ATTR和CTL按8位寄存器定义。实现代码应使用`volatile uint8_t`访问，避免32位读改写误改相邻IRQ字段。

初始化时至少读取并校验：

```text
num_interrupts = CLICINFO.num_interrupt
ctlbits        = CLICINFO.CLICINTCTLBITS
```

T22参考代码把`num_interrupt=0`解释为256。当前RT-Thread实现应按E902定义处理该编码，同时对循环上限和向量表容量做一致性断言。

### 4. `mtvec`、`mtvt`和`shv`

这三个概念分别回答：

| 配置 | 回答的问题 |
| --- | --- |
| `mtvec` | 公共陷阱入口在哪里 |
| `mtvt` | 硬件向量表在哪里 |
| `shv` | 某一路中断是否使用硬件向量表 |

#### 4.1 通用MODE编码与E902实际实现

`mtvec`的通用MODE编码为：

| MODE | 通用含义 | 入口规则 |
| --- | --- | --- |
| 0 | Direct | 异常和中断都进入BASE |
| 1 | 标准Vectored | 异常进入BASE，中断进入`BASE + 4*cause` |
| 2 | 保留 | 不使用 |
| 3 | CLIC | 由CLIC规则和`shv`选择入口 |

**关键区别**：这张表描述编码含义，不代表E902可以在四种模式间切换。E902硬件固定`MODE=3`，软件不可设置成0或1。

T22启动代码仍执行：

```asm
la   a0, Default_Handler
ori  a0, a0, 3
csrw mtvec, a0
```

低两位写3与硬件固定值一致，但真正需要软件提供的是正确、64字节对齐的`mtvec.BASE`。实现代码应写入后读回校验，而不是把`| 3`解释为“运行时打开CLIC”。

#### 4.2 公共入口`mtvec`

在E902 CLIC模式下，`mtvec[31:6] << 6`用于：

- 所有同步异常的公共入口。
- `shv=0`的非向量中断公共入口。
- NMI的统一异常入口。

入口必须64字节对齐：

```asm
.align 6
e902_trap_entry:
```

#### 4.3 向量表`mtvt`

`mtvt`保存64字节对齐的硬件向量表基址。向量表是32位处理函数入口地址数组，不是`BASE + 4*irq`处直接放置的指令：

```text
entry_address = mtvt_base + 4 * irq_id
handler       = *(uint32_t *)entry_address
PC            = handler
```

这是一种“两级跳转”：CPU先从向量表读取函数地址，再跳转到该地址执行。

如果实现256个表项，向量表需要1024字节。链接脚本必须保证：

- 64字节对齐。
- 表项数量覆盖硬件可能使用的IRQ。
- 向量段不会被`--gc-sections`删除。
- 运行时可写向量表位于可写内存；只读向量表则必须在链接时完成初始化。

#### 4.4 每路`shv`

`CLICINTATTR[i].shv`由M模式初始化软件设置：

```text
shv = 0：非向量中断，进入mtvec公共入口
shv = 1：硬件向量中断，通过mtvt[irq_id]读取入口
```

`shv`不是硬件自动选择，也不是写入向量地址时自动设置。

T22参考实现的`csi_vic_set_vector(irq, handler)`只更新：

```c
((uint32_t *)mtvt)[irq] = handler;
```

它不会配置`shv`、`trig`、`CLICINTIE`、`MINTTHRESH`或`mstatus.MIE`。

这个接口只负责“登记入口地址”，单看职责划分并不错误。向量地址与触发方式是两类不同配置：更换ISR函数通常不应悄悄改变硬件信号的边沿/电平特性。T22裸机的完整使用方式是先由`clic_config()`统一禁用所有IRQ、清pending、设置`ATTR=0x1`和最低控制编码，再由各驱动调用`csi_vic_set_vector()`和`csi_vic_enable_irq()`；`ERRB0_IRQn`等特殊中断则在全局初始化中覆盖ATTR。因此该工程不是完全没有配置属性，而是把属性配置放在了另一处。

问题在于这种依赖全局默认值的关系不够显式：驱动只看`set_vector + enable`无法确认本路是否确实为`shv=1`、触发极性是否匹配，也无法证明IRQ号小于`CLICINFO`报告的数量和实际`mtvt`容量。`csi_vic_set_vector()`只检查`irq < 1024`，这个上限明显不能替代硬件数量与向量表边界检查。

推荐保留底层接口的单一职责，同时提供一个更高层的注册/激活流程：

```text
irq_disable(irq)
    -> 清除外设历史状态和可软件清除的pending
    -> irq_install_vector(irq, entry)
    -> irq_configure(irq, shv, trig, level/priority)
    -> 校验irq范围、入口地址和向量表容量
    -> 必要的fence
    -> irq_enable(irq)
```

`shv`、`trig`和默认level最好由SoC中断描述或驱动配置明确提供，并在首次注册或设备激活、且IRQ仍处于禁用状态时写入。不要让最底层的`set_vector()`隐式猜测触发方式；也不要允许上层只写向量和IE就绕过属性校验。

### 5. 触发、仲裁与pending

#### 5.1 `trig`触发类型

`CLICINTATTR[i].trig`位于bit[2:1]。E902 Rev.10给出的实现语义是：

| 条件 | E902含义 |
| --- | --- |
| `trig[0] = 0` | 电平触发；外部源为高时IP为1，为低时IP为0 |
| `trig[0] = 1, trig[1] = 0` | 上升沿触发 |
| `trig[0] = 1, trig[1] = 1` | 下降沿触发 |

因此，`00`是当前明确使用的高电平配置，`01`是上升沿，`11`是下降沿。手册没有把`10`定义为“负电平触发”；当前方案不使用该组合。配置前必须确认SoC连接到CLIC的信号极性，不能仅根据外设寄存器名称或其他CLIC版本的枚举猜测。

#### 5.2 level、priority和阈值

`CLICINTCTL`高位中只有`CLICINTCTLBITS`位真正实现，未实现的低位在送给CPU的8位编码中补1。`CLICCFG.nlbits`决定有效位如何划分：

```text
CLICINTCTL = [ level bits | priority bits | hardware-tied bits ]
```

- level决定请求的抢占等级：线程态同时pending时通常先选择更高level；ISR允许嵌套时，只有更高level请求才能抢占当前ISR。
- priority只决定多个同level、且都可以响应的请求先处理谁，不赋予同level中断嵌套能力。
- `MINTTHRESH`屏蔽不高于阈值的请求。
- 编码越高，仲裁等级越高。
- 当`nlbits > CLICINTCTLBITS`时，硬件实际只能使用已实现的`CLICINTCTLBITS`位。

T22参考代码把`nlbits`设置为`CLICINTCTLBITS`，因此所有有效控制位都作为level，没有额外的同level priority位。此时名为`csi_vic_set_prio()`的接口实质上改变的是level编码，阅读代码时不要被函数名误导。

中断level与嵌套的关系可以按进入和重新开中断两个阶段理解：

1. CPU接受IRQ A后，把进入前的MIL保存到`mcause.MPIL`，再把`MINTSTATUS.MIL`更新为A的level。
2. 硬件同时清零`mstatus.MIE`。此时无论来了多高level的普通IRQ，都不能嵌套，只能保持pending。
3. A的入口软件保存本层GPR、`mepc`、`mstatus`、`mcause`等会被覆盖的状态后，如果主动重新设置`mstatus.MIE=1`，才真正允许嵌套。
4. 重新开中断后，IRQ B必须同时高于`MINTTHRESH`和当前`MINTSTATUS.MIL`，才能抢占A；同level或更低level只能等待。
5. B执行`mret`时，硬件用其`mcause.MPIL`恢复A的MIL，继续执行A；A最终`mret`再恢复进入A之前的MIL。

例如A的level为2、B为3、C也为2：A进入后当前MIL为2。A若不重新打开`mstatus.MIE`，B和C都不能嵌套；A保存完整现场后重新打开`mstatus.MIE`，B可以抢占A，而C仍必须等A退出。由此可见，level只给出“开嵌套后谁有资格抢占”，`mstatus.MIE`才是“当前是否允许发生嵌套”的总开关。

T22裸机的`save_epc_mstatus()`先把本层`mepc/mstatus/mcause`保存到深度为4的全局数组，然后调用`__enable_irq()`，所以它确实主动允许更高level中断嵌套。当前RT-Thread第一版设计不这样做：异常和ISR入口保持`mstatus.MIE=0`，先完成单层中断和上下文切换验证。

#### 5.3 一个IRQ何时进入CPU

在当前全M模式运行场景中，普通可屏蔽中断至少需要满足：

```text
中断源有效或已锁存pending
    AND CLICINTIE[i] = 1
    AND 请求level高于MINTTHRESH
    AND 请求level高于当前MINTSTATUS.MIL
    AND mstatus.MIE = 1
```

priority用于同一可响应level内的仲裁，不等同于RT-Thread线程优先级。更准确的表述是：CLIC level决定中断请求的抢占等级，并在软件重新打开`mstatus.MIE`后限制中断嵌套；RT-Thread线程优先级决定退出中断并完成调度后运行哪个线程。

#### 5.4 pending清除规则

pending的清除方式与触发类型、`shv`有关：

| 中断类型 | CLIC pending行为 | 软件责任 |
| --- | --- | --- |
| 电平中断 | `CLICINTIP`反映外部电平，通常只读 | 必须清除外设内部源，使中断线撤销 |
| 边沿且`shv=1` | CPU接受向量中断时，CLIC响应握手清除IP | 仍应处理并清除外设自身状态 |
| 边沿且`shv=0` | 普通入口不会自动清IP；对`mnxti`进行有效读写claim可清除 | 正确使用`mnxti`并处理外设状态 |

只读`mnxti`可查询待处理中断入口，但不会清除IP；用于claim的有效CSR读改写操作才具有相应副作用。当前第一版RT-Thread实现选择硬件向量方式，不依赖`mnxti`咬尾。

必须区分两层状态：

```text
外设状态：UART状态位、DW Timer TxIntStatus等
CLIC状态：CLICINTIP[i]
```

清除CLIC pending不等于清除外设中断源。电平源未撤销时，`mret`后会立即再次进入。

### 6. 硬件进入和退出陷阱时做什么

#### 6.1 硬件完成的动作

E902接受异常或普通中断后，硬件主要完成CSR状态更新和入口跳转：

1. 将返回地址写入`mepc`。
2. 将异常号或IRQ号写入`mcause.ExceptionCode`，并更新Interrupt标志。
3. 异常需要附加信息时更新`mtval`。
4. 把原`mstatus.MIE`保存到`MPIE`，随后清零`MIE`。
5. 保存进入陷阱前的特权状态，并切换到M模式。
6. CLIC中断时把原`MINTSTATUS.MIL`保存到`mcause.MPIL`，再更新当前MIL。
7. 根据事件类型和`shv`，从`mtvec`或`mtvt`选择下一条取指地址。

边沿pending是否在此阶段清除，按5.4节的条件处理。

#### 6.2 硬件不会完成的动作

E902硬件不会：

- 把`x1-x15`自动压入内存栈。
- 自动调整或切换`sp`。
- 建立C语言函数栈帧。
- 把CSR复制到软件栈或普通内存。
- 清除UART、DW Timer等外设内部状态。
- 调用`rt_interrupt_enter()`或`rt_interrupt_leave()`。
- 选择RT-Thread下一个线程。
- 保存线程栈并执行线程上下文切换。

因此，在裸机源码中看不到“硬件保存`x1-x15`”的汇编是正常的：硬件从未做这件事。若入口是编译器中断函数，保存GPR的指令由编译器生成；若入口是手写汇编，则必须由汇编显式实现。

#### 6.3 `mret`完成的动作

软件恢复必要GPR和被保存的CSR后执行`mret`。E902主要完成：

1. `PC = mepc`。
2. 恢复进入陷阱前的特权状态。
3. `mstatus.MIE = mstatus.MPIE`。
4. CLIC中断时`MINTSTATUS.MIL = mcause.MPIL`。

`mret`只恢复架构状态，不知道RT-Thread线程对象。若返回到另一个线程，必须在执行`mret`前由CPU port切换`sp`并恢复目标线程现场。

## 第三部分 软件入口与现场

### 7. 三条入口路径

#### 7.1 同步异常

```text
当前指令产生异常
    -> 硬件更新mepc/mcause/mtval/mstatus
    -> PC = mtvec.BASE
    -> 汇编公共入口保存x1-x15及必要CSR
    -> C异常分发函数
    -> 诊断、修复、重试、跳过、终止或复位
    -> 恢复现场
    -> mret
```

异常公共入口必须在调用任何C函数前建立有效栈帧。异常恢复策略必须按`mcause`分类，不能把所有异常都当成可跳过断点。

#### 7.2 CLIC硬件向量中断

前提：

```text
E902固定CLIC模式
CLICINTATTR[i].shv = 1
```

路径：

```text
外设产生请求
    -> CLIC按IP/IE/level/threshold仲裁
    -> E902更新mepc/mcause/mstatus和CLIC level
    -> handler = *(mtvt + 4 * irq_id)
    -> PC = handler
    -> 入口保存软件需要的现场
    -> 调用ISR并清除外设源
    -> 恢复现场
    -> mret
```

**当前方案**：第一版普通IRQ采用该路径。

#### 7.3 CLIC非向量中断

前提：

```text
E902固定CLIC模式
CLICINTATTR[i].shv = 0
```

路径：

```text
CLIC请求满足仲裁条件
    -> PC = mtvec.BASE
    -> 公共入口保存现场
    -> 软件对mnxti执行有效访问并取得入口地址
    -> 调用对应ISR
    -> 可继续使用mnxti完成咬尾
    -> 恢复现场
    -> mret
```

该方式便于统一入口和中断咬尾，但`mnxti`副作用、嵌套level和入口分发更复杂。第一版移植不采用该路径。

### 8. GPR和CSR由谁保存

#### 8.1 手写汇编入口

T22裸机`Default_Handler`显式保存`x1-x15`以及`mepc`、`mstatus`和`mcause`，处理完成后按相反顺序恢复并执行`mret`。这适合公共异常入口，因为现场布局完全由软件控制。

手写入口应遵循：

1. 在破坏临时寄存器前先保存它们。
2. 在调用C函数前建立符合ABI的栈。
3. 保存宏、恢复宏和初始线程栈共享同一偏移定义。
4. 恢复`sp`时避免覆盖仍要使用的基址寄存器。
5. 最后一条陷阱返回指令必须是`mret`。

#### 8.2 编译器生成的向量ISR

T22向量ISR使用：

```c
__attribute__((interrupt("machine")))
```

编译器通常会：

- 在当前栈上分配ISR栈帧。
- 保存本函数及其调用链可能破坏的GPR。
- 恢复这些GPR。
- 使用`mret`而不是普通`ret`返回。

编译器不保证机械保存全部`x1-x15`，生成结果还会受优化级别和函数调用影响。普通C函数没有该属性时通常以`ret`结束，不能直接作为`mtvt`硬件向量入口。

因此每种ISR属性和优化配置都应反汇编验证，不能只看C源码判断现场是否完整。

硬件向量入口通常有两种合法组织方式：

| `mtvt`表项指向 | C处理函数类型 | 谁执行`mret` |
| --- | --- | --- |
| 带`interrupt("machine")`属性的C ISR | 编译器中断函数 | 编译器生成的ISR结尾 |
| 手写汇编包装入口 | 普通C分发/处理函数 | 汇编包装入口 |

两种方式不能混用。若汇编包装入口调用一个带中断属性的C函数，该函数可能直接`mret`，从而跳过包装入口的恢复逻辑；若把普通C函数直接装入`mtvt`，它又会以`ret`错误返回。

#### 8.3 中断嵌套时的CSR保护

第一次进入中断后，硬件已经把状态放在`mepc/mstatus/mcause`中。如果ISR始终保持`mstatus.MIE=0`并直接返回，单层中断通常不需要为了返回再次复制这些CSR。

如果ISR重新打开`MIE`允许嵌套，下一层中断会覆盖当前层的`mepc`、`mstatus`、`mcause`和CLIC扩展状态。因此必须先把当前层状态保存到本层栈帧，再允许嵌套。

所有退出路径必须成对恢复。提前`return`、热补丁分支、错误路径和异常路径都要单独审计。

**当前方案**：第一版禁止中断嵌套。先验证单层异常、软件中断、DW Timer和上下文切换；之后若确有实时性需求，再设计可证明正确的嵌套栈帧。

#### 8.4 责任边界

| 状态或动作 | 负责者 |
| --- | --- |
| 更新返回地址、原因和陷阱状态CSR | E902硬件 |
| 保存`x1-x15` | 手写汇编入口或编译器ISR序言 |
| 为嵌套复制会被覆盖的CSR | 异常/中断入口软件 |
| 建立C函数可使用的栈环境 | 汇编入口或编译器 |
| 识别并清除外设内部中断源 | 对应外设ISR |
| 维护RT-Thread中断嵌套计数 | `rt_interrupt_enter/leave` |
| 决定下一个运行线程 | RT-Thread调度器 |
| 保存线程现场、切换`sp`、恢复目标现场 | E902 CPU port |

### 9. RV32E线程上下文

#### 9.1 RV32E寄存器约束

当前E902使用RV32EC和ILP32E，只有`x0-x15`：

| 寄存器 | ABI名称 | 主要用途 |
| --- | --- | --- |
| `x0` | `zero` | 常数0，不需要保存 |
| `x1` | `ra` | 返回地址 |
| `x2` | `sp` | 栈指针 |
| `x3` | `gp` | 全局指针 |
| `x4` | `tp` | 线程指针 |
| `x5-x7` | `t0-t2` | 临时寄存器 |
| `x8-x9` | `s0-s1` | 被调用者保存寄存器 |
| `x10-x15` | `a0-a5` | 参数和返回值 |

任何访问`x16-x31`的通用RISC-V port都会在RV32E上产生非法指令或无法汇编，不能直接使用。

#### 9.2 统一现场格式

玄铁RTOS SDK的RT-Thread `e902mt`参考port使用17个32位槽：

```text
x1-x15 + mepc + mstatus
```

本工程经过重新审查后没有机械复制该布局，而是让线程上下文与现有异常/中断入口共用20个32位槽、共80字节：

```text
x1-x15 + mepc + mstatus + mcause + mtval + reserved
```

选择统一现场的原因是IRQ 3继续经过已验证的CLIC公共入口和C分发。普通IRQ返回同一现场时，需要恢复该现场保存的`mcause`，使`mret`使用其中的`MPIL`恢复进入中断前的Machine Interrupt Level。IRQ 3切换到另一个线程时则不同：`mcause`描述的是当前正在退出的IRQ 3，不属于目标线程；切换`sp`后必须保留当前CSR中的IRQ 3 `mcause`，不能用目标线程现场中的值覆盖它。`mtval`和`reserved`虽然不参与普通线程调度，但保留它们可以避免异常、普通IRQ和线程切换维护多套偏移。

必须保证以下三处使用同一布局：

- 异常/中断保存宏。
- 上下文恢复宏。
- `rt_hw_stack_init()`构造的新线程初始栈。

新线程初始现场至少应正确设置：

- `mepc = thread_entry`。
- `a0 = parameter`。
- `ra = thread_exit`。
- `gp = __global_pointer$`。
- `mstatus.MPP = M`。
- `mstatus.MPIE = 1`，使首次`mret`后进入正常中断状态。

栈地址必须满足`ilp32e`工具链ABI和上下文宏的对齐要求。应通过ELF属性、编译器输出和断言确认，不要把其他RISC-V ABI的栈对齐假设直接搬入。

#### 9.3 为什么编译器ISR栈帧不能代替线程上下文

RT-Thread在中断返回前可能选择另一个线程。此时：

- `sp`要切换到另一个线程栈。
- 目标线程不是从当前ISR函数调用进来的。
- 初始线程必须能从人工构造的现场第一次启动。
- 现场格式不能随编译器版本或优化等级变化。

所以RT-Thread必须使用固定、手写的上下文格式；编译器ISR序言只适合保护单个C ISR的调用现场。

## 第四部分 T22与RT-Thread实现方案

### 10. T22已确认的中断现状

#### 10.1 CPU与CLIC配置

| 项目 | 已确认值 |
| --- | --- |
| CPU ISA | RV32EC |
| ABI | ILP32E |
| 当前特权模式 | M模式 |
| 陷阱入口框架 | E902固定CLIC，`mtvec.MODE=3` |
| `mtvec.BASE` | `Default_Handler` |
| `mtvt.BASE` | `__Vectors` |
| 普通IRQ默认`shv` | 1，硬件向量 |
| 普通IRQ默认`trig` | `00`，高电平有效 |
| `CLIC_BASE` | `0xE0800000` |
| `CLICINFO` | `0x00600050` |
| CLIC总IRQ数 | 80 |
| `CLICINTCTLBITS` | 3 |

T22 `Reset_Handler`的相关顺序是：

```text
清零x1-x15
    -> mtvec.BASE = Default_Handler
    -> mtvt.BASE = __Vectors
    -> 初始化sp、.data和.bss
    -> SystemInit()
        -> clic_config()
    -> main()
    -> 驱动安装向量并使能具体IRQ
    -> main_init()末尾设置mstatus.MIE=1
```

`clic_config()`当前执行：

1. 从`CLICINFO`读取有效控制位数，并把`nlbits`设为该值。
2. 读取中断数量；字段为0时按256处理。
3. 禁用每路IRQ并尝试清pending。
4. 默认设置`ATTR=0x1`，即`shv=1, trig=00`，高电平有效。
5. 默认设置最低控制编码。
6. 对部分IRQ调整level或触发属性，例如`ERRB0_IRQn`使用`ATTR=0x3`。
7. 不写`MINTTHRESH`，也不写`mie` CSR；前者沿用进入本程序前的硬件状态，后者不参与当前CSI CLIC单路使能流程。

#### 10.2 T22 RX主要IRQ

| IRQ | 中断源 |
| --- | --- |
| 3 | Machine Software Interrupt |
| 7 | Core Timer Interrupt |
| 16-18 | UART0-UART2 |
| 19-21 | I2C0-I2C2 |
| 22-25 | SPI控制器 |
| 26 | GPIO |
| 27 | DW Timer 8通道共享中断 |
| 28-32 | ECC、Timeout和MISC |
| 33-50 | MIPI RX及MIPI0-MIPI3相关中断 |
| 51-53 | I2C/GPIO/Other Data |
| 54-58 | Protocol通道 |
| 66-70 | DVP、PLL、控制面错误和ERRB |

这些编号属于T22 SoC集成事实，应集中定义在`soc/t22-serdes/`，不能写死在E902通用上下文汇编中。

#### 10.3 裸机参考代码的适用边界

T22裸机代码证明了启动、CLIC硬件向量和外设中断可工作，但它不是可直接复制的RTOS port：

- `Default_Handler`适合公共异常入口，但其异常路径默认按指令长度跳过故障指令；RTOS异常策略必须按原因分类。
- 向量ISR依赖编译器`interrupt("machine")`现场，不是统一线程上下文。
- `save_epc_mstatus()`使用固定深度的全局数组保存嵌套CSR，缺少栈式现场的自然隔离。
- 某些ISR存在提前`return`路径，可能绕过与函数尾成对的恢复操作；迁移时必须逐条审计。
- 参考代码允许嵌套，而当前第一版RT-Thread方案关闭嵌套。
- 当前`__Vectors`覆盖IRQ 0-79，而`csi_vic_set_vector()`只检查IRQ小于1024；这个接口检查不能证明向量表没有越界。
- 向量表容量、`CLICINFO`中断数量和SoC最高IRQ需要由链接期及运行时共同校验。

正确用法是复用已验证的寄存器地址、IRQ映射和外设清除顺序，同时重新设计RT-Thread所需的统一入口和线程现场。

### 11. 目标RT-Thread中断架构

#### 11.1 分层职责

| 层次 | 职责 |
| --- | --- |
| `libcpu/risc-v/e902/` | CSR、异常入口、CLIC核心访问、中断开关、线程栈和上下文切换 |
| `soc/t22-serdes/` | T22 IRQ编号、外设基地址、时钟和复位关系 |
| `drivers/` | DW Timer、UART等控制器状态处理和中断源清除 |
| `boards/` | 选择控制台、Tick通道和板级资源 |
| RT-Thread内核 | 中断嵌套计数、就绪队列、调度决策和线程状态 |

CPU port中不应出现T22 UART、DW Timer等外设寄存器；驱动中也不应定义线程上下文布局。

#### 11.2 第一版入口

当前CLIC基础入口采用：

```text
同步异常
    -> mtvec.BASE
    -> e902_exception_entry
    -> 保存完整RV32E现场
    -> exception_dispatch
    -> 恢复、停机或复位

普通CLIC中断
    -> mtvt[irq_id]
    -> e902_irq_entry
    -> 保存完整RV32E现场和必要CSR
    -> e902_irq_dispatch()
    -> 已注册处理函数清除中断源
    -> 恢复现场
    -> mret
```

当前80个T22向量项均指向同一个公共入口，第一版保守保存全部RV32E GPR，不为不同IRQ生成不同栈帧。公共入口在C分发前后调用`rt_interrupt_enter()`和`rt_interrupt_leave()`：RT-Thread镜像使用`src/irq.c`强实现维护嵌套计数，裸机镜像使用CPU port弱空实现保持既有测试行为。`a0`属于调用者保存寄存器，因此传给`e902_irq_dispatch()`的现场指针必须在`rt_interrupt_enter()`返回后由`sp`重新装载，不能假设前一次C调用会保留`a0`。

普通硬件IRQ的现场用于保证当前ISR能够正确返回，并不在其中直接切换线程。若ISR执行期间的内核路径调用`rt_schedule()`，调度器会在`rt_interrupt_nest != 0`时调用`rt_hw_context_switch_interrupt()`记录切换请求并置位IRQ 3；`rt_interrupt_leave()`本身只维护中断嵌套计数。当前IRQ随后恢复自身现场并`mret`，pending的IRQ 3再进入固定线程现场路径，完成实际`sp`切换。即使两种现场第一版采用相同寄存器集合，其职责也不同。

#### 11.3 调度决策与实际切换

需要严格区分三件事：

1. **内核状态变化**：ISR或线程使某个高优先级线程就绪。
2. **调度决策**：RT-Thread选择`from`线程和`to`线程。
3. **上下文切换**：CPU port保存`from`现场、切换`sp`并恢复`to`现场。

第一版已经参考玄铁`e902mt` port实现Machine Software Interrupt，即IRQ 3延后执行常规上下文切换：

```text
RT-Thread选出to线程
    -> CPU port记录from/to线程栈指针地址
    -> 设置CLICINTIP[3]
    -> IRQ 3入口保存from线程完整现场
    -> 把from->sp更新为当前sp
    -> sp = to->sp
    -> 保留当前IRQ 3的mcause
    -> 恢复to线程的mstatus、mepc和GPR
    -> mret进入to线程
```

这里不能在切换`sp`后从`to`线程现场恢复`mcause`。新线程的初始`mcause`为0，若它覆盖当前IRQ 3的`mcause`，`mret`将无法使用当前中断的`MPIL`退出CLIC level；后续同level的IRQ 3会保持pending而不能再次响应。实现中，`.Lirq_restore`为普通IRQ恢复保存的`mcause`后进入公共恢复代码；`rt_hw_context_switch_to()`在第一次启动线程时装载初始`mcause`；IRQ 3切换线程则直接进入`e902_context_restore`，保留当前CSR中的`mcause`。玄铁`e902mt`参考port的任务切换路径同样只从目标线程恢复GPR、`mepc`和`mstatus`。

IRQ 3的pending地址按寄存器模型计算为：

```text
CLIC_BASE + 0x1000 + 4 * 3 = 0xE080100C
```

实现中应使用命名宏计算，避免在多处散落魔法地址。

当IRQ 3尚未处理又出现新的调度请求时，切换请求需要合并：

- 第一次请求记录真正被打断的`from`线程。
- 后续请求不能覆盖`from`，只更新最终应运行的`to`线程。
- IRQ 3完成切换后再清除“切换待处理”标志。

否则连续调度可能把`from`错误改成尚未运行的中间线程，最终把现场保存到错误的线程栈指针中。设置`CLICINTIP[3]`时应使用8位volatile访问，并按参考实现和总线要求加入必要的`fence`，保证pending写在返回调度路径前可见。

并非所有线程启动都经过软件中断：

- 第一个线程由`rt_hw_context_switch_to()`直接装载目标`sp`、恢复初始现场并`mret`。
- 后续普通线程切换由IRQ 3处理程序执行实际寄存器切换。
- 若调度发生在硬件ISR中且`MIE`仍关闭，IRQ 3先保持pending，待当前ISR返回后再被接受。

因此，准确表述应是：“调度器做出切换决策并请求CPU port切换；除首次线程启动外，第一版E902方案在Machine Software Interrupt处理程序中执行实际上下文切换。”

独立双线程验证的方法、理论切换次数、寄存器特征值和失败分析见[《E902线程上下文切换验证》](e902-context-switch-validation.md)。CPU port已经完成目标板验证；RT-Thread调度器和系统Tick必要代码已接入，目标板调度验证仍属于第8阶段。

#### 11.4 当前特权模式

当前第一版RT-Thread实现统一使用：

```text
RT-Thread内核 = M模式
应用线程      = M模式
驱动和ISR     = M模式
```

暂不引入U模式、PMP、系统调用和用户地址检查。

### 12. 可靠初始化顺序

CLIC基础层、线程上下文和RT-Thread ISR边界当前按以下顺序实现：

1. `startup.S`清除`mstatus.MIE`，安装64字节对齐的异常公共入口，并写`mtvec.BASE | 3`。
2. 完成`.data`、`.bss`和板级初始化；链接脚本已保留64字节对齐、80项的`mtvt`向量表。
3. T22初始化入口校验向量表的精确大小。
4. 读取`CLICINFO`，校验中断数量和E902支持的`CLICINTCTLBITS`范围。
5. 在全局中断关闭时禁用所有已实现`CLICINTIE`。
6. 写入并读回`mtvt`。
7. 令`CLICCFG.nlbits=CLICINTCTLBITS`、`MINTTHRESH.mth=0`，并回读两个字段。
8. 清空描述符表，为可用IRQ设置默认`shv=1`、高电平触发和最低控制编码。
9. T22层确认硬件IRQ数量覆盖SoC最高IRQ 70。
10. 调用`e902_context_switch_init()`，把IRQ 3注册为正边沿硬件向量中断，清除旧pending并使能该路。
11. 注册其他目标IRQ：先保持该路关闭；边沿触发源清除旧pending，再安装处理函数并配置`shv`、`trig`和`CLICINTCTL`。
12. 处理函数和清源路径就绪后，只打开实际使用的`CLICINTIE[i]`。
13. 所有入口、向量和已启用中断源就绪后，最后恢复或打开`mstatus.MIE`。

这个顺序的目标是：全局中断打开时，入口、栈、向量、处理函数和清源逻辑都已经有效。

当前实现已经检查`mtvt`对齐和读回、`CLICINFO`范围、IRQ边界、T22最高IRQ覆盖范围，以及向量表链接地址和精确大小。接入RT-Thread前还应继续增加以下断言或诊断：

- `mtvec`读回的BASE和固定MODE值符合预期。
- 每个实际启用的向量表项不是0且落在可执行地址范围。
- `sp`处于有效RAM范围并满足ABI对齐。

### 13. DW Timer系统Tick方案

**当前方案**：RT-Thread系统Tick使用T22 DW Timer，不使用E902 Core Timer。

#### 13.1 已确认资源

| 项目 | 当前确认值 |
| --- | --- |
| 基地址 | `0x00103400` |
| 通道数量 | 8 |
| 通道步长 | `0x14` |
| 公共状态寄存器 | `0x001034A0`，低8位分别对应8个通道 |
| 共享中断 | CLIC IRQ 27 |
| 输入时钟 | 当前固定为200 MHz APB |
| Tick通道 | 零基通道0，即TIMER1，基地址`0x00103400` |
| 周期模式 | `TxControl.MODE=1` |
| 清中断 | 读取对应通道`TxEOI` |

每通道寄存器布局：

| 偏移 | 寄存器 |
| --- | --- |
| `0x00` | `TxLoadCount` |
| `0x04` | `TxCurrentValue` |
| `0x08` | `TxControl` |
| `0x0C` | `TxEOI` |
| `0x10` | `TxIntStatus` |

Board选择通道0，与当前T22产品参考配置`CONFIG_SYS_TICK_DW_TIMER=0`一致。200 MHz APB下，目标Tick频率为`1000 Hz`时装载值为：

```text
load_count = 200000000 / 1000 = 200000
```

当前实现只接受能够整除APB时钟的频率，避免因整数取整产生隐藏的长期漂移。若后续修改APB时钟，必须重新配置Timer，不能继续沿用旧装载值。

#### 13.2 分层与所有权

DW Timer路径分为四层：

| 层次 | 职责 |
| --- | --- |
| `drivers/timer/dw_apb_timer/` | 单通道寄存器布局、配置、启停、状态和EOI |
| `soc/t22-serdes/t22_serdes_timer.c` | 8通道资源、活动掩码、共享IRQ 27注册与分发 |
| `boards/t22-deserializer-evb/board_tick.c` | 选择通道0并提供系统Tick回调接口 |
| `rt-thread`接入层 | 周期回调调用`rt_tick_increase()`，公共IRQ入口维护内核中断边界 |

通道0归Board系统Tick独占。其他定时功能必须通过SoC共享分发层注册独立通道，不能直接替换IRQ 27处理函数，也不能操作通道0寄存器。

#### 13.3 共享IRQ职责

IRQ 27配置为高电平硬件向量中断。处理函数读取公共状态寄存器快照并遍历全部置位通道：

```text
进入IRQ 27
    -> 读取公共状态寄存器低8位
    -> 遍历全部pending通道
    -> 对每个pending通道读取TxEOI
    -> EOI完成后调用该通道的已注册回调
    -> 没有有效回调的通道被停止，避免中断风暴
    -> 最后一个活动通道停止后关闭CLICINTIE[27]
    -> 返回
```

读取`TxEOI`清除的是DW Timer外设内部中断源；CLIC高电平pending随后随外部中断线撤销。只清`CLICINTIP[27]`不能替代读取`TxEOI`。

处理函数不能在完成第一个通道后提前返回，否则同时pending的后续通道得不到清源，共享中断线可能持续有效。外设清源放在回调之前，避免回调执行较长时IRQ 27一直保持有效。

第6阶段的裸机回调不调用`rt_tick_increase()`。RT-Thread应用注册独立Tick回调，每个TIMER1周期调用一次`rt_tick_increase()`；`rt_interrupt_enter()`和`rt_interrupt_leave()`由E902公共IRQ入口统一负责，Tick回调不能重复维护嵌套计数。

#### 13.4 初始化与启停顺序

```text
t22_serdes_irq_init()
    -> 初始化并屏蔽8个DW Timer通道，读取EOI清除旧状态
    -> 注册IRQ 27处理函数、配置高电平触发和控制字段
    -> 配置TIMER1装载值与周期模式
    -> 启动TIMER1并使能CLICINTIE[27]
    -> 最后打开mstatus.MIE
```

Timer启动、活动掩码更新和CLIC使能在全局中断关闭时完成。这样不会出现Timer已经产生请求，而共享描述符或回调尚未就绪的窗口。停止时先屏蔽并关闭目标通道、读取EOI，再根据剩余活动通道决定是否关闭IRQ 27。

#### 13.5 验证方案与当前状态

必要代码和独立验证应用已经完成，目标板周期中断验证最终输出`PASS`。具体构建命令、测试流程、日志格式和失败码见[《E902 DW Timer周期中断验证》](e902-timer-validation.md)。验证结果确认：

- TIMER1连续100个周期符合1 kHz目标频率和允许误差。
- TIMER1和TIMER2共用IRQ 27时均能得到分发和清源。
- 停止TIMER1时，仍在运行的TIMER2和共享IRQ保持有效。
- TIMER1能够再次启动，全部通道停止后pending状态为0。

长时间Tick漂移、丢Tick、无回调防护路径和高负载下的共享pending仍属于后续稳定性验证，不由本次短时功能自测覆盖。

## 第五部分 验证目标

架构文档不展开测试实现；异常和CLIC测试见[《E902异常与CLIC验证》](e902-interrupt-validation.md)，DW Timer测试见[《E902 DW Timer周期中断验证》](e902-timer-validation.md)，线程上下文测试见[《E902线程上下文切换验证》](e902-context-switch-validation.md)。

| 验证目标 | 关注内容 |
| --- | --- |
| 同步异常入口 | `mtvec`、64字节对齐、异常现场保存和`mcause/mepc/mtval`诊断 |
| 现场完整性 | RV32E `x1-x15`、`sp`、栈边界和现场布局一致性 |
| 异常返回 | CSR恢复、GPR恢复和`mret`行为 |
| 断点恢复 | `c.ebreak`与32位`ebreak`的指令长度及`mepc`处理 |
| CLIC软件中断 | `mtvt`、`shv`、`CLICINTIE[3]`和pending清除 |
| 不可恢复异常 | 记录现场后停机，不无条件跳过非法指令或访问错误 |
| 周期中断 | DW Timer频率、共享IRQ 27分发、停止隔离和再次启动 |
| 线程上下文 | 初始栈、首次`mret`、IRQ 3往返切换、寄存器和栈完整性 |
| 后续系统验证 | RT-Thread调度、ISR退出调度和长期运行 |

## 附录

### A. Core Timer背景

E902 Core Timer使用IRQ 7，相关寄存器为：

| 寄存器 | 地址 |
| --- | --- |
| `MTIMECMPLO` | `0xE0004000` |
| `MTIMECMPHI` | `0xE0004004` |
| `MTIMELO` | `0xE000BFF8` |
| `MTIMEHI` | `0xE000BFFC` |

RV32读取64位`MTIME`需要采用“高-低-高”方式保证一致性；更新`MTIMECMP`时通常先把高32位写为`0xFFFFFFFF`，再写低32位和最终高32位，避免中间值误触发中断。

这些内容用于理解E902核心中断资源，不是当前RT-Thread系统Tick方案。

### B. 术语速查

| 术语 | 含义 |
| --- | --- |
| Trap | 异常和中断的统称 |
| Exception | 当前指令同步产生的事件 |
| Interrupt | 外部、软件或计时事件异步产生的事件 |
| NMI | Non-Maskable Interrupt，不受`mstatus.MIE`屏蔽 |
| CLINT | Core-Local Interruptor，核心本地中断资源 |
| CLIC | Core-Local Interrupt Controller |
| `mtvec` | Machine公共陷阱入口及MODE寄存器 |
| `mtvt` | CLIC硬件向量表基址寄存器 |
| `shv` | Selective Hardware Vectoring，每路硬件向量选择 |
| `trig` | 每路中断的触发类型和极性 |
| `mstatus.MIE` | M模式可屏蔽中断的全局开关 |
| `mie` | 标准Machine中断类别使能CSR，不等同于`mstatus.MIE`或`CLICINTIE` |
| `CLICINTIE` | CLIC每路IRQ的独立使能寄存器 |
| `CLICINTCTLBITS` | 每个`CLICINTCTL`实际实现的高位数量 |
| `MINTTHRESH` | M模式CLIC中断接收阈值 |
| MIL | `MINTSTATUS`中的当前Machine Interrupt Level |
| level | CLIC抢占等级，只有更高level可在重新开`MIE`后嵌套 |
| priority | 同level请求的仲裁优先级 |
| pending | 中断请求等待处理的状态 |
| claim | CPU或软件接受一个待处理中断 |
| 咬尾 | 不完全退出公共入口就继续处理下一个中断 |
| lockup | 陷阱处理中再次发生不可安全处理的异常时，CPU进入的硬件锁定状态 |
| `mret` | 从Machine陷阱处理返回 |
| ISR | Interrupt Service Routine，中断服务函数 |

### C. 自检问题

完成本文学习后，应能回答：

1. 为什么T22启动代码写`mtvec | 3`，却不能说是软件“打开了CLIC模式”？
2. 当前程序为什么是M模式，内嵌汇编与特权模式有什么关系？
3. `shv`由谁设置，在什么阶段设置？
4. 异常为什么进入`mtvec`，硬件向量中断为什么通过`mtvt`？
5. 硬件进入中断时保存了哪些CSR，为什么没有自动形成线程栈帧？
6. 电平、向量边沿和非向量边沿中断的pending分别如何清除？
7. 为什么编译器ISR栈帧不能直接作为RT-Thread线程上下文？
8. 为什么清除CLIC pending不等于清除外设中断源？
9. 为什么DW Timer IRQ 27必须检查多个通道？
10. 调度器做出切换决策后，真正的寄存器切换在哪里执行？
11. 哪一次线程启动不经过IRQ 3，为什么？
12. 为什么通用RV32I port不能直接用于RV32E？
13. `mstatus.MIE`、`mie` CSR和`CLICINTIE[i]`分别控制哪一层？
14. `CLICINTCTLBITS`和`nlbits`如何共同决定level与priority？
15. 为什么更高level中断也必须等ISR重新打开`mstatus.MIE`后才能嵌套？
16. 为什么`csi_vic_set_vector()`不应自行猜测`trig`，但高层激活流程仍应显式配置中断属性？

### D. 依据与代码对应关系

| 内容 | 主要依据 |
| --- | --- |
| 异常号、中断号、硬件进入/返回 | 《玄铁E902 R3S0用户手册》4.1、4.2、4.3 |
| CLIC寄存器、`nlbits`、`trig`、pending | 用户手册第10章 |
| `mtvec`、`mtvt`、`mnxti`和扩展CSR | 用户手册16.2、16.3 |
| T22入口和向量表 | 裸机工程`__Chip_E902/src/startup.S` |
| T22 CLIC默认配置 | 裸机工程`ctrl_subsystem/src/system.c` |
| T22中断嵌套开关和CSR保护 | 裸机工程`ctrl_subsystem/src/common.c` |
| T22 IRQ编号 | 裸机工程`__Chip_E902/include/soc.h` |
| 异常处理中再次异常与lockup | 玄铁RTOS SDK `solutions/bare_core_lockup/` |
| IRQ 3的`ATTR=0x3`参考配置 | 玄铁RTOS SDK `components/chip_riscv_dummy/src/arch/e902mt/system.c` |
| DW Timer寄存器和清源 | 裸机工程`knl_timer.h`、`knl_timer.c` |
| 断点指令长度与恢复原则 | 本文第2.1节；具体测试见独立验证文档 |
| RV32E RT-Thread现场和IRQ 3切换 | 玄铁RTOS SDK `components/rtthread/libcpu/riscv/e902mt/` |
| 中断态调度接口选择 | 本工程`rt-thread/src/scheduler.c`、`rt-thread/src/irq.c` |

参考实现只证明一种可行路径。最终移植应以当前E902手册、T22 SoC集成、实际工具链反汇编和板上验证共同形成闭环。
