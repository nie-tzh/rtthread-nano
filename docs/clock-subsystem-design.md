# Clock子系统设计、Linux CCF与T22落地

## 1. 文档目标

本文基于Linux 6.8.9 Common Clock Framework（CCF）分析Clock子系统的核心对象、分层方式和调用路径，并据此定义RT-Thread Nano工程中的轻量级Clock框架及T22实现。

目标不是移植Linux的设备模型、设备树、动态注册表和并发管理，而是保留以下稳定设计：

- Consumer只持有时钟句柄，不直接依赖PLL、分频器或门控寄存器。
- Clock Core提供统一API并把请求转发给Provider。
- Provider通过`struct clk_hw`和`struct clk_ops`描述硬件能力。
- 固定频率、分频、门控和复用等时钟类型可以逐步独立扩展。
- SoC描述时钟树和硬件事实，Board选择并初始化当前产品使用的资源。

当前实现以生产代码的真实需求为边界，只实现频率查询和T22启动时钟配置，不为测试代码或尚不存在的动态调频、门控需求预留运行时机制。

## 2. 基本概念

### 2.1 Clock Provider

Clock Provider是产生、选择、分频或门控时钟的硬件控制器及其驱动，例如晶振、PLL、固定分频器、时钟MUX和gate。

Provider知道：

- 时钟寄存器和位定义。
- 时钟的父子关系。
- 如何计算、设置或启停频率。
- 当前硬件支持哪些操作。

### 2.2 Clock Consumer

Clock Consumer是使用时钟的设备驱动，例如UART、Timer、I2C和SPI。

Consumer只需要表达：

- 获取本设备对应的时钟。
- 查询输入频率。
- 在硬件确有门控能力时启用或关闭时钟。

Consumer不应直接读取PLL选择位，也不应复制SoC频率常量来计算波特率或计数周期。

### 2.3 Clock Core

Clock Core位于Consumer与Provider之间，提供统一的`clk_*`接口，解析父时钟并调用Provider的`clk_ops`。

这一分层把“设备需要多少频率”与“时钟由什么硬件产生”解耦。

## 3. Linux 6.8.9 CCF

Linux CCF主要由以下文件实现：

```text
include/linux/clk.h                  Consumer API和struct clk
include/linux/clk-provider.h         Provider API、struct clk_hw和clk_ops
drivers/clk/clk.c                    Clock Core、拓扑、计数和频率传播
drivers/clk/clkdev.c                 非设备树Consumer查找
drivers/clk/clk-fixed-rate.c         固定频率Provider
drivers/clk/clk-fixed-factor.c       固定比例Provider
drivers/clk/clk-divider.c            分频Provider
drivers/clk/clk-gate.c               门控Provider
drivers/clk/clk-mux.c                父时钟选择Provider
```

### 3.1 分层关系

```text
UART / Timer / I2C等Consumer Driver
        |
        | clk_get_rate / clk_prepare_enable
        v
Clock Core
        |
        | clk_ops
        v
PLL / Divider / Mux / Gate Provider
        |
        v
时钟控制寄存器
```

### 3.2 struct clk

`struct clk`是Consumer持有的时钟句柄。Linux为每个Consumer维护独立句柄，其中包含其对应的`clk_core`、设备、连接名和频率约束。

Consumer不通过`struct clk`访问硬件字段，只把它传给`clk_get_rate()`、`clk_prepare_enable()`等公共接口。

### 3.3 struct clk_core

`struct clk_core`是Linux CCF的内部对象，负责：

- 保存当前频率和父时钟关系。
- 构建全局时钟树。
- 管理prepare/enable引用计数。
- 串行化时钟配置。
- 传播频率变化和通知。
- 关联Provider的`struct clk_hw`。

它是完整Linux系统所需的运行时管理层，不是每个轻量级RTOS都必须复制的结构。

### 3.4 struct clk_hw

`struct clk_hw`是Clock Core连接Provider私有对象的桥梁。Linux要求具体Provider把它嵌入自己的硬件结构：

```c
struct clk_fixed_rate
{
    struct clk_hw hw;
    unsigned long fixed_rate;
};
```

Provider回调收到`struct clk_hw *`后，可以恢复为具体硬件对象并访问寄存器或描述数据。这种“公共对象嵌入具体对象”的设计避免Core理解每一种时钟硬件。

### 3.5 struct clk_ops

Linux `struct clk_ops`按硬件能力提供可选回调，主要包括：

```text
prepare / unprepare         可睡眠的准备与释放
enable / disable            原子上下文中的门控
recalc_rate                 根据硬件和父频率重新计算频率
round_rate / determine_rate 选择可实现的目标频率
set_rate                    修改频率
get_parent / set_parent     查询或切换父时钟
```

Provider只实现真实硬件支持的操作。固定频率时钟只需要`recalc_rate`，没有必要提供空的`set_rate`或gate回调。

### 3.6 clk_get_rate调用路径

Linux调用路径可概括为：

```text
Consumer
    -> clk_get_rate(struct clk *)
        -> clk_core_get_rate_recalc()
            -> 取得或刷新clk_core中的rate
                -> Provider clk_ops.recalc_rate()
```

对于固定频率Provider：

```text
clk_fixed_rate_recalc_rate()
    -> 返回struct clk_fixed_rate.fixed_rate
```

对于divider或mux，Core先获得父时钟频率，Provider再根据寄存器计算当前输出频率。

### 3.7 注册与Consumer关联

Linux Provider通过`clk_hw_register()`、`devm_clk_hw_register()`及各类专用注册函数加入CCF。设备树中的`clocks`和`clock-names`或`clkdev`表负责把Consumer关联到Provider输出。

Consumer典型路径为：

```text
devm_clk_get(dev, "baud")
    -> 解析设备与连接名
    -> 获得struct clk
    -> clk_get_rate(clk)
```

这一动态关联适合支持热插拔、多个设备实例和复杂时钟树的Linux系统。

## 4. RT-Thread Nano轻量化设计

当前RT-Thread Nano源码没有可直接复用的Clock子系统，因此项目建立最小公共层，但不重新实现Linux完整CCF。

### 4.1 保留的Linux语义

```text
struct clk             Consumer句柄
struct clk_hw          Provider硬件对象
struct clk_ops         Provider操作集
clk_get_rate()         Consumer频率查询API
clk_hw_get_rate()      Core内部父频率传播
struct clk_fixed_rate  通用固定频率Provider
```

### 4.2 主动裁剪的机制

首版不实现：

- 全局Clock注册表和名称查找。
- 设备树、ACPI或`clkdev`解析。
- 动态内存和设备托管资源。
- `clk_core`频率缓存、锁和通知链。
- prepare/enable引用计数。
- 动态set-rate、mux、divider和gate操作。
- 运行时错误恢复和静态拓扑环检测。

这些能力只有出现真实Consumer和硬件需求时才扩展，不能为了形式接近Linux而提前增加代码。

### 4.3 核心数据结构

```c
struct clk
{
    const struct clk_hw *hw;
};

struct clk_ops
{
    unsigned long (*recalc_rate)(const struct clk_hw *hw,
                                 unsigned long parent_rate);
};

struct clk_hw
{
    const struct clk_ops *ops;
    const struct clk_hw *parent;
};
```

- `struct clk`只表达Consumer到Provider输出的连接。
- `struct clk_hw`保存Provider操作和可选父时钟。
- `struct clk_ops`目前只提供真实使用的`recalc_rate`。
- `parent`为后续divider、mux和gate保留时钟树的基本表达能力，不引入注册表或缓存。
- 静态时钟拓扑使用`const`对象，全部驻留ROM，不为Clock框架增加常驻RAM。

### 4.4 Nano调用路径

```text
Consumer
    -> clk_get_rate(clk)
        -> clk_hw_get_rate(clk->hw)
            -> 递归获取parent_rate
            -> hw->ops->recalc_rate(hw, parent_rate)
```

没有`recalc_rate`的透明节点直接继承父时钟频率；固定频率Provider忽略`parent_rate`并返回自身频率。

## 5. T22 Clock Provider

### 5.1 Provider输出

T22首版提供三路时钟：

| Clock ID | 频率 | 当前Consumer |
| --- | ---: | --- |
| `T22_CLK_AHB` | 320 MHz | CPU周期测量及后续AHB设备 |
| `T22_CLK_APB` | 200 MHz | DW UART、DW APB Timer |
| `T22_CLK_SPI` | 200 MHz | 后续SPI控制器 |

三路输出在启动完成后保持固定，因此使用通用`struct clk_fixed_rate`，不实现动态调频和门控。

### 5.2 eFuse策略

旧固件根据`chip_reality_apb_clk`选择200 MHz或25 MHz路径。当前工程统一按eFuse值为0处理：

```text
AHB = 320 MHz
APB = 200 MHz
SPI = 200 MHz
```

Provider不读取eFuse寄存器，不增加eFuse条件分支。该选择是当前产品构建策略，而不是运行时探测结果。

### 5.3 初始化路径

```text
startup.S
    -> board_early_init()
        -> t22_clk_init()
            -> 使能system clock 2
            -> 通过T22受保护CSR写接口配置PLL选择位
        -> board_reset_init()
        -> board_pinctrl_init()
        -> board_early_console_init()       可选
```

时钟必须先于Reset、Pinctrl和UART初始化建立，避免外设在输入时钟未确定时访问寄存器。

### 5.4 UART Consumer路径

```text
board_early_init()
    -> clk_get_rate(t22_clk_get(T22_CLK_APB))
    -> 写入dw_apb_uart_config.clock_hz

board_early_console_init() / board_uart_init()
    -> DW UART根据clock_hz计算波特率除数
```

UART不再直接引用APB频率常量。

### 5.5 Timer Consumer路径

```text
t22_serdes_timer_config_periodic()
    -> clk_get_rate(t22_clk_get(T22_CLK_APB))
    -> load_count = timer_clock_rate / frequency_hz
```

Timer不再直接引用APB频率常量，后续时钟拓扑变化只需要修改Provider和静态连接。

## 6. 目录与职责

```text
drivers/clk/
    clk.c                       Clock Core
    clk-fixed-rate.c            通用固定频率Provider
    include/clk.h               Consumer API
    include/clk-provider.h      Provider API
    t22_clk/
        t22_clk.c               T22 PLL配置和时钟输出
        include/t22_clk.h       T22 Clock ID及静态获取接口

soc/t22-serdes/
    include/t22_serdes.h        SoC地址、目标频率和受保护CSR接口

boards/t22-deserializer-evb/
    board.c                     初始化T22 Provider并把时钟交给Consumer
```

Clock寄存器流程位于`drivers/clk/`，地址和目标频率属于SoC事实，当前Board负责选择并启动T22 Provider。

## 7. 后续扩展原则

### 7.1 增加新Consumer

UART0、UART1、I2C或SPI接入时，Consumer只取得对应`struct clk`并调用`clk_get_rate()`。不得重新引入`T22_SERDES_*_CLOCK_HZ`计算外设参数。

### 7.2 增加divider或mux

当硬件需要运行时分频或父时钟切换时：

1. 定义嵌入`struct clk_hw`的具体Provider对象。
2. 实现对应`clk_ops`。
3. 通过`clk_hw.parent`连接父时钟。
4. 仅在真实Consumer需要时扩展`round_rate`、`set_rate`或父时钟API。

### 7.3 增加gate

只有确认存在独立门控寄存器和生命周期需求后，才向Core增加`clk_enable()`、`clk_disable()`及Provider回调。若没有并发Consumer和共享gate，不提前引入引用计数。

### 7.4 增加Serializer

若Serializer与Deserializer的PLL寄存器语义一致，复用同一T22 Provider；若地址、选择编码或输出拓扑不同，只增加芯片描述数据，不复制Clock Core和Consumer API。

## 8. 完成标准

- Debug与Release版本均可构建。
- 启动日志和正式UART波特率正确。
- RT-Thread Tick保持1 kHz，任务调度周期不变。
- UART和Timer生产代码不再直接读取APB频率宏。
- T22时钟初始化中不存在eFuse读取和eFuse条件分支。
- Clock Core不包含设备树、动态注册或未使用的控制接口。
