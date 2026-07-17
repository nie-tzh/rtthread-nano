# RT-Thread Nano E902移植进度

## 总体移植阶段

| 阶段 | 状态 | 阶段目标 |
| --- | --- | --- |
| 1. 确认目标硬件和工具链参数 | 已完成 | 确认CPU指令集、ABI、工具链和编译参数 |
| 2. 搭建最小编译系统 | 已完成 | 建立分层、可复现且不依赖IDE的构建系统 |
| 3. 最小裸机启动验证 | 已完成 | 验证入口、栈、`.data`、`.bss`和C函数运行环境 |
| 4. 板级初始化与串口输出 | 已完成 | 完成早期时钟、引脚、UART初始化和上板日志输出 |
| 5. 建立异常与CLIC中断机制 | 已完成 | 同步异常和CLIC IRQ 3软件中断均已完成上板验证 |
| 6. 实现DW Timer并验证周期中断 | 未开始 | 使用T22 DW Timer产生RT-Thread系统Tick |
| 7. 实现E902线程栈与上下文切换 | 未开始 | 完成RV32E线程栈构造和软件中断上下文切换 |
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

具体构建命令、实测日志和失败码见[《E902异常与CLIC验证》](e902-interrupt-validation.md)。第5阶段已经完成，下一步进入第6阶段的T22 DW Timer周期中断验证。

## 设计边界和已知风险

- RT-Thread官方通用RISC-V上下文代码使用`x16-x31`，不能直接用于RV32E。
- 调试变量地址由链接结果决定。需要固定地址时应使用专用section，并由链接脚本显式放置。
- RT-Thread系统Tick使用T22 DW Timer，不使用E902 Core Timer。
- T22 RX的DW Timer基地址为`0x00103400`，最多8个通道，通道步长为`0x14`，共用CLIC IRQ 27。
- DW Timer使用当前APB时钟。第6阶段需要确认实际APB频率、Tick通道以及该通道是否与现有裸机功能冲突。
- 调度器负责选择下一个线程，E902软件中断入口负责真正保存和恢复上下文。
- CLIC核心机制、T22中断号和外设中断源清除属于不同层次，实现时不能混为一个模块。
