# Reset子系统设计与T22实施方案

## 1. 文档目标

本文分析Linux 6.8.9 Reset子系统的设计思想和主要实现机制，并据此定义RT-Thread Nano工程中Reset子系统的首版实现方案。

目标不是照搬Linux的设备模型、设备树和动态资源管理，而是保留其经过验证的provider/consumer分层、接口语义和命名方式，为UART、I2C、SPI、GPIO等外设提供统一、精简且可扩展的复位接口。

本文作为Reset子系统实现和代码评审的依据。实现阶段若需要改变本文确定的接口或职责边界，应先更新设计结论。

## 2. 基本概念

### 2.1 Reset line

Reset line是Reset Controller输出到外设模块的物理复位信号。一条reset line通常对应复位寄存器中的一个bit，但也可能由一个硬件动作控制多条物理信号或一段固定时序。

### 2.2 Reset control

Reset control是软件用于控制一条或一组reset line的方法。常见形式包括：

- 电平控制：软件分别执行assert和deassert。
- 脉冲控制：软件触发一次reset操作，硬件或驱动完成完整复位脉冲。
- 组合控制：一次操作影响多个相关模块。

### 2.3 Reset controller

Reset Controller是提供多路reset control的硬件模块，例如SoC系统控制器中的外设软复位寄存器。

### 2.4 Reset consumer

Reset consumer是被复位信号控制的外设，例如UART、I2C、SPI、GPIO或DMA控制器。

## 3. Linux Reset子系统

Linux Reset子系统主要由以下文件实现：

```text
include/linux/reset.h                 Consumer公共接口
include/linux/reset-controller.h      Controller驱动接口
drivers/reset/core.c                  Reset Core
drivers/reset/reset-simple.c          通用寄存器位控制器
drivers/reset/reset-k210.c            RISC-V K210控制器实例
Documentation/driver-api/reset.rst    设计与接口说明
```

### 3.1 总体分层

```text
UART/I2C/SPI等Consumer Driver
        |
        | reset_control_reset/assert/deassert
        v
Reset Core
        |
        | reset_control_ops
        v
Reset Controller Driver
        |
        v
Reset寄存器或硬件时序
```

Consumer只表达“复位、进入复位或解除复位”的需求，不知道寄存器地址、bit位置、有效电平和脉冲时序。

### 3.2 reset_control_ops

Controller驱动通过`struct reset_control_ops`提供硬件操作：

```c
struct reset_control_ops
{
    int (*reset)(struct reset_controller_dev *rcdev,
                 unsigned long id);
    int (*assert)(struct reset_controller_dev *rcdev,
                  unsigned long id);
    int (*deassert)(struct reset_controller_dev *rcdev,
                    unsigned long id);
    int (*status)(struct reset_controller_dev *rcdev,
                  unsigned long id);
};
```

- `reset`：产生完整复位脉冲。
- `assert`：使reset line进入有效状态。
- `deassert`：解除reset line。
- `status`：查询reset line当前是否有效。

这些回调均由Controller驱动实现。Reset Core不直接操作硬件寄存器，也不假定复位信号的有效电平。

### 3.3 reset_controller_dev

`struct reset_controller_dev`表示一个Reset Controller provider。其核心信息是：

```c
struct reset_controller_dev
{
    const struct reset_control_ops *ops;
    unsigned int nr_resets;
    /* Linux还包含设备模型、DT、链表和模块管理字段。 */
};
```

- `ops`定义控制器操作方法。
- `nr_resets`定义控制器提供的reset control数量。
- Linux中的`of_xlate`负责把设备树reset specifier转换为Controller内部ID。

Controller驱动初始化该结构后，通过`reset_controller_register()`注册到Reset Core。

### 3.4 reset_control

`struct reset_control`是Consumer持有的不透明句柄。Linux内部核心关系可简化为：

```c
struct reset_control
{
    struct reset_controller_dev *rcdev;
    unsigned int id;
};
```

Linux的实际结构还包含引用计数、独占/共享状态、数组标记以及共享reset所需的计数器。

该结构表达：

```text
某个Consumer使用哪个Reset Controller中的哪一路reset control
```

### 3.5 Provider与Consumer关联

Linux设备树使用phandle和reset specifier描述关联：

```dts
reset: reset-controller {
    #reset-cells = <1>;
};

uart2 {
    resets = <&reset UART2_RESET_ID>;
    reset-names = "uart";
};
```

Consumer调用：

```c
rstc = devm_reset_control_get_exclusive(dev, "uart");
```

Reset Core完成以下工作：

```text
根据reset-names找到resets条目
    -> 找到Reset Controller provider
    -> 将specifier转换为reset ID
    -> 检查独占或共享关系
    -> 返回reset_control句柄
```

没有设备树的平台也可以通过静态lookup表建立同样的关联。

### 3.6 Consumer操作

Consumer获得`reset_control`后使用统一接口：

```c
reset_control_assert(rstc);
reset_control_deassert(rstc);
reset_control_reset(rstc);
reset_control_status(rstc);
```

Linux DW 8250 UART驱动的典型流程是：

```text
获取可选、独占的reset control
    -> 解除复位
    -> 初始化UART
    -> 设备退出时重新assert复位
```

UART驱动不会直接包含SoC复位寄存器定义。

### 3.7 assert、deassert与reset

三个接口不能简单视为同一操作的不同名称：

```text
assert
    保证设备处于复位状态

deassert
    保证设备离开复位状态

reset
    产生一次完整复位动作，通常为assert + delay + deassert
```

如果硬件只支持自清除的脉冲复位，Controller可以只实现`reset`。如果硬件允许软件直接控制reset line，则可以实现`assert/deassert/status`。

Linux Core不会自动使用`assert + deassert`代替缺失的`reset`回调。复位时序属于Controller驱动，所需延时也应由Controller实现。

### 3.8 独占与共享

Linux支持两类reset control：

- 独占Reset：一个Consumer直接控制reset line，操作立即影响硬件。
- 共享Reset：多个Consumer共享reset line，Reset Core通过引用计数协调assert和deassert。

共享Reset中，只有第一次deassert和最后一次匹配的assert会真正操作硬件。共享脉冲Reset还需要`triggered_count`和`rearm`语义。

该机制用于复杂SoC资源共享，不是所有嵌入式系统都需要。

### 3.9 Bulk、Optional与资源管理

Linux还支持：

- Bulk Reset：按组获取和操作多路reset control。
- Optional Reset：平台没有声明reset时返回空句柄，Consumer代码无需条件编译。
- devm管理：设备解绑时自动释放reset control。
- 动态注册、引用计数和模块生命周期管理。

这些能力依赖Linux设备模型和动态资源管理，不应直接复制到当前Nano工程。

### 3.10 K210实例

Linux RISC-V K210 Reset驱动与T22硬件模型较为接近：

```text
assert    -> 修改外设复位寄存器对应bit
deassert  -> 恢复对应bit
reset     -> assert -> 延时10 us -> deassert
status    -> 读取复位寄存器对应bit
```

K210驱动将`reset_controller_dev`嵌入私有结构，并通过`container_of`取得寄存器访问对象。该组织方式适合作为T22 provider设计的主要参考。

## 4. Nano Reset子系统设计

### 4.1 设计原则

Nano实现遵循以下原则：

1. 保留Linux provider/consumer分层和接口语义。
2. Consumer不得直接访问SoC复位寄存器。
3. 通用Reset Core不依赖RT-Thread设备模型。
4. Reset Controller不注册为字符设备或`rt_device`。
5. 使用静态对象描述固定硬件资源，不引入动态内存。
6. 首版只实现当前生产代码需要的独占脉冲Reset。
7. 不为了未来需求提前实现共享、Bulk、Optional和动态lookup。
8. 接口和结构体不增加`rt_`前缀，以便复用于其他裸机或RTOS环境。
9. 返回值使用`int`和负errno风格，与现有Pinctrl公共层保持一致。
10. `assert/deassert/status`保留为已分析的扩展语义，首版没有Consumer需求时不生成对应生产代码。

### 4.2 首版核心数据结构

#### reset_control_ops

```c
struct reset_control_ops
{
    int (*reset)(struct reset_controller_dev *rcdev,
                 uint32_t id);
};
```

首版只保留生产代码实际使用的`reset`回调，接口名称与Linux保持一致。后续出现“长期保持复位”“解除复位”或状态查询的真实Consumer时，再按Linux语义增加`assert/deassert/status`，无需改变现有对象关系。

#### reset_controller_dev

```c
struct reset_controller_dev
{
    const struct reset_control_ops *ops;
    uint32_t nr_resets;
    void *driver_data;
};
```

- `ops`：控制器操作方法。
- `nr_resets`：控制器可表达的reset ID范围。
- `driver_data`：具体驱动私有对象。

`driver_data`沿用当前Nano Pinctrl公共层的组织方式，避免通用Reset Core依赖RT-Thread专用`container_of`宏。首版不包含全局链表、设备树翻译、模块owner和注册状态。

#### reset_control

```c
struct reset_control
{
    struct reset_controller_dev *rcdev;
    uint32_t id;
};
```

该对象由Board静态定义，用于关联具体外设与SoC reset ID。

### 4.3 首版公共接口

```c
int reset_control_reset(const struct reset_control *rstc);
```

Reset Core只负责：

- 检查`reset_control`和Controller基本有效性。
- 检查`id < nr_resets`。
- 检查`reset`operation是否存在。
- 调用Controller operation并透传返回值。

Reset Core不负责：

- 推断reset有效电平。
- 自动生成复位延时。
- 用`assert/deassert`模拟缺失的`reset`回调。
- 操作T22受保护CSR。
- 管理Board资源选择。

### 4.4 静态资源关联

Nano当前没有设备树和通用platform bus，Board使用静态对象完成Consumer关联：

```c
/* board_reset.c */
static struct t22_reset board_reset_controller;

static const struct reset_control board_uart2_reset_control =
{
    .rcdev = &board_reset_controller.rcdev,
    .id = T22_SERDES_RESET_UART2
};

void board_reset_init(void)
{
    t22_reset_init(&board_reset_controller,
                   &t22_deserializer_reset_data);
}

int board_uart2_reset(void)
{
    return reset_control_reset(&board_uart2_reset_control);
}

/* board.c */
void board_early_init(void)
{
    t22_clk_init();
    board_reset_init();
    board_pinctrl_init();
}
```

其中`t22_deserializer_reset_data`由Deserializer芯片层定义，包含该芯片的复位寄存器资源和reset ID范围。Board不应直接引用`T22_SERDES_CSR`、寄存器偏移或写保护细节。

该方式对应Linux设备树中的：

```dts
resets = <&reset_controller T22_SERDES_RESET_UART2>;
```

后续增加UART0、I2C0或SPI时，只需要增加新的`reset_control`静态对象，不修改Reset Core和T22 provider流程。

## 5. T22 Reset Controller方案

### 5.1 硬件模型

当前T22 Deserializer代码通过`CSR_SOFT_RESET`寄存器控制外设复位：

```c
t22_serdes_csr_update32(&soft_reset, mask, 0U);
t22_serdes_csr_update32(&soft_reset, mask, mask);
```

基于当前已验证流程，可以暂时理解为：

```text
bit = 0    assert reset
bit = 1    deassert reset
```

复位寄存器偏移属于芯片变体数据，不能放入RX/TX公共资源层。当前已确认Deserializer使用`0x24`，Serializer使用`0x20`；两者的UART2复位位均为bit 10。其他复位位仍应按各自芯片数据确认，并需要确认：

- Reset bit的有效电平。
- assert到deassert之间是否要求最小脉宽。
- 复位后是否需要等待外设时钟稳定。
- 是否存在只能脉冲触发、不能读取状态的reset ID。

禁止为了模仿K210而随意加入固定延时。若手册要求最小脉宽，延时应放在T22 provider的`reset`回调中。

### 5.2 T22私有对象

建议使用：

```c
struct t22_reset_soc_data
{
    uintptr_t reset_reg;
    uint32_t nr_resets;
};

struct t22_reset
{
    struct reset_controller_dev rcdev;
    const struct t22_reset_soc_data *soc;
};
```

`t22_reset_soc_data`描述具体T22芯片变体的硬件事实，由对应芯片目录提供；`t22_reset`是Controller运行对象，由Board选择并初始化。RX/TX可以共享Reset Controller驱动和已确认一致的Reset ID，但必须分别提供自己的复位寄存器地址。如果T22受保护CSR不能由普通MMIO写入，则provider继续调用现有`t22_serdes_csr_update32()`，不在Reset Core复制写保护流程。

### 5.3 T22 operations

```text
t22_reset_reset(rcdev, id)
    -> mask = BIT(id)
    -> 受保护地清除soft-reset bit以assert
    -> 按硬件手册执行必要延时
    -> 受保护地设置soft-reset bit以deassert
```

首版不导出仅供测试使用的复位计数、最后复位ID或原始寄存器状态。若后续增加`status`操作，其返回语义应对齐Linux：`1`表示asserted，`0`表示deasserted，负值表示错误，而不是直接返回原始bit值。

### 5.4 并发与寄存器更新

多路外设reset共享同一个`CSR_SOFT_RESET`寄存器，因此必须使用read-modify-write，不能直接覆盖整个寄存器。

如果Reset API可能在线程和中断上下文中同时调用，T22 provider必须保证该寄存器更新不会互相覆盖。首版实现时应检查现有`t22_serdes_csr_update32()`是否已经满足单核中断并发要求；若不满足，应在SoC受保护CSR访问层统一解决，而不是在每个Reset operation中复制锁逻辑。

## 6. 目录与职责

建议目录：

```text
drivers/reset/
    driver.mk
    reset.c
    include/reset.h
    t22_reset/
        t22_reset.c
        include/t22_reset.h

soc/t22-serdes/
    include/t22_serdes_reset.h       RX/TX已确认共用的Reset ID
    t22-deserializer/
        t22_deserializer_reset.h
        t22_deserializer_reset.c     Deserializer Reset资源数据
    t22-serializer/
        t22_serializer_reset.h
        t22_serializer_reset.c       未来Serializer Reset资源数据

boards/t22-deserializer-evb/
    board_reset.c                    设备与Reset line的静态关联
    include/board_reset.h            Board Reset接口
```

职责边界：

| 层次 | 职责 |
| --- | --- |
| Reset Core | 统一Consumer API并分发到Controller ops |
| T22 Reset Driver | 实现T22复位有效电平和脉冲时序；需要时再扩展状态读取 |
| SoC | 提供reset ID、CSR地址及受保护访问能力 |
| Board | 将具体设备关联到对应reset control |
| UART/I2C/SPI Driver | 调用Reset Consumer API，不访问T22 CSR |

## 7. UART2迁移方案

迁移前UART2直接调用：

```c
t22_serdes_reset(T22_SERDES_RESET_UART2);
```

迁移后调用：

```c
board_uart2_reset();
```

初始化顺序建议为：

```text
board_early_init
    -> t22_clk_init
    -> board_reset_init
        -> t22_reset_init
    -> board_pinctrl_init
    -> board_early_console_init
        -> select UART2 default pinctrl state
        -> board_uart2_reset
            -> reset_control_reset(UART2)
        -> initialize DW UART
```

正式UART初始化继续调用`board_uart2_reset()`，由`board_reset.c`统一使用同一个`board_uart2_reset_control`对象，不创建第二套复位接口。

当前迁移已删除旧`t22_serdes_reset()`，生产代码统一通过Reset Core进入T22 Controller，避免出现绕过公共接口的第二套复位路径。

## 8. 首版非目标

以下能力不进入首版实现：

- Reset Controller动态注册和注销。
- 全局Controller链表和名称查找。
- 设备树、ACPI或platform bus关联。
- 动态内存和引用计数。
- shared reset及其deassert计数。
- bulk reset和reset control数组。
- optional reset的空句柄语义。
- devm自动释放。
- 系统重启、看门狗复位和reset reason管理。

这些能力只有出现真实Consumer需求时才扩展，不能为了形式上接近Linux提前引入。

## 9. 实施步骤

### 步骤1：增加Reset Core

- 新增`reset.h`和`reset.c`。
- 定义三个核心结构和一个`reset_control_reset()` Consumer接口。
- 保持通用层无RT-Thread和T22依赖。

### 步骤2：增加T22 Reset Controller

- 实现T22 `reset`operation。
- 复用受保护CSR更新接口。
- 在具体芯片目录提供各自的Reset寄存器资源数据。
- 明确有效电平和reset pulse时序。

### 步骤3：建立Board静态关联

- 在`board_reset.c`初始化T22 Reset Controller实例。
- 在`board_reset.c`定义UART2 reset control及Board接口。
- `board.c`只负责编排`board_reset_init()`调用顺序。
- 确保Early Console使用前Controller已初始化。

### 步骤4：迁移UART2

- 使用`reset_control_reset()`替代直接SoC调用。
- 移除或收敛旧复位接口。
- 不修改DW UART通用寄存器驱动。

### 步骤5：验证与收口

- 执行Demo Debug和Release编译。
- 检查新增代码无未使用接口和无RT-Thread依赖。
- 上板验证Early Console和正式UART控制台输出。
- 验证5 ms和10 ms线程持续调度。
- 检查RAM和ROM增量。

## 10. 完成标准

Reset子系统首版完成需要同时满足：

1. UART代码不再直接操作或引用T22 soft-reset寄存器。
2. Reset Core、T22 provider、SoC资源和Board映射职责清晰。
3. Consumer只持有`reset_control`并调用标准接口。
4. 通用Reset Core不依赖RT-Thread、Board或T22头文件。
5. 不引入动态内存、全局注册表和未使用的共享管理能力。
6. Debug和Release编译通过。
7. Early Console及正式UART在目标板运行正常。
8. Demo双线程调度日志持续正常输出。
9. 文档与最终代码接口保持一致。

## 11. 设计结论

Nano Reset子系统采用Linux的核心分层，但不复制Linux的动态设备管理机制：

```text
reset_control
    表达Consumer使用哪一路复位资源

reset_controller_dev
    表达哪个Controller提供这些资源

reset_control_ops
    隔离具体寄存器、有效电平和复位时序
```

T22首版只实现静态、独占、单路Consumer控制。该模型足以支持UART、I2C、SPI、GPIO等后续外设，同时保持当前Nano工程所要求的低资源占用和清晰扩展路径。
