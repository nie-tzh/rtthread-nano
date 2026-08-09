# Pinctrl子系统设计、Linux实现与T22落地

## 1. 文档目标

本文基于Linux 6.8.9源码分析Pinctrl子系统的设计思想、核心对象和主要调用链，并说明RT-Thread Nano工程中轻量级Pinctrl框架的设计取舍及T22平台的具体实现。

本文用于回答四个问题：

1. Pinctrl为什么需要独立于UART、I2C、SPI和GPIO驱动存在。
2. Linux如何把Controller能力、Board连接关系和Consumer状态选择分开。
3. Nano保留了Linux模型中的哪些核心语义，又主动裁剪了哪些机制。
4. T22的通用Controller驱动、芯片变体数据和Board状态表分别负责什么。

本文描述的是生产代码架构，不以测试程序为设计输入。后续增加GPIO、I2C、SPI或T22 Serializer支持时，应优先沿用本文确定的职责边界。

## 2. 基本概念

### 2.1 Pin、Pad与MFP

Pin通常表示芯片对外可控制的物理引脚，Pad更强调芯片内部的电气连接单元。不同芯片手册也可能使用ball、finger、IO或MFP等名称。

T22将可复用引脚称为MFP。Nano Pinctrl统一使用`pin`作为软件抽象，T22驱动再将pin编号解释为具体MFP。

### 2.2 Pinmux

Pinmux用于选择一个pin当前连接到哪个片上功能，例如：

```text
MFP4 -> GPIO
MFP4 -> UART2 RX
MFP4 -> I2C SCL
```

这些功能通常互斥。一个pin在同一时刻只能处于一种有效复用关系。

### 2.3 Pinconf

Pinconf用于设置pin的电气属性，例如：

- 上拉、下拉或无上下拉。
- 驱动能力。
- 输入使能。
- Schmitt触发。
- slew rate。
- 开漏或高阻模式。

Pinmux回答“连接到哪个功能”，Pinconf回答“以什么电气方式工作”。

### 2.4 Function与Group

Linux使用Function和Group描述复用能力：

- Function表示UART、I2C、SPI等功能选择。
- Group表示完成该功能所需的一组pin。
- 一个Function可以支持多个Group，用于表达同一外设的多组可选引脚。

简单控制器也可以将每一个pin视为单pin Group。

### 2.5 State

State表示一个Consumer在某个生命周期阶段需要的完整引脚状态。Linux定义的通用状态包括：

```text
default    正常工作状态
init       驱动probe期间的临时状态
idle       运行时空闲状态
sleep      低功耗休眠状态
```

一个State可以同时包含多项mux和config设置。

### 2.6 Provider与Consumer

- Pinctrl Provider是pin controller驱动，知道pin能力和硬件寄存器。
- Pinctrl Consumer是UART、I2C、SPI、MMC、GPIO等需要使用引脚的设备。

Consumer只选择状态，不应直接写MFP、IOMUX或Pad配置寄存器。

## 3. Linux Pinctrl子系统

Linux 6.8.9的主要实现文件包括：

```text
include/linux/pinctrl/pinctrl.h       Controller描述和pinctrl_ops
include/linux/pinctrl/pinmux.h        Pinmux Controller operations
include/linux/pinctrl/pinconf.h       Pinconf Controller operations
include/linux/pinctrl/consumer.h      Consumer公共接口
include/linux/pinctrl/machine.h       静态pinctrl_map
drivers/pinctrl/core.c                Pinctrl Core
drivers/pinctrl/core.h                Core内部对象
drivers/pinctrl/pinmux.c              Pinmux管理和冲突处理
drivers/pinctrl/pinconf.c             Pinconf应用
drivers/pinctrl/devicetree.c          设备树到map的转换
drivers/base/pinctrl.c                Driver Core与Pinctrl连接
Documentation/driver-api/pin-control.rst
```

### 3.1 总体分层

```text
Consumer Driver或Driver Core
        |
        | pinctrl_get / lookup_state / select_state
        v
Pinctrl Core
        |
        +---- Pinmux Core ----> pinmux_ops
        |
        +---- Pinconf Core ---> pinconf_ops
        v
Pin Controller Driver
        |
        v
IOMUX、Pad Control和相关系统寄存器
```

Linux Pinctrl Core不理解具体SoC寄存器。它负责管理对象、解析映射、选择状态、处理pin所有权，并调用Controller operations。

### 3.2 pinctrl_pin_desc

`struct pinctrl_pin_desc`描述Controller提供的每一个pin：

```c
struct pinctrl_pin_desc
{
    unsigned int number;
    const char *name;
    void *drv_data;
};
```

- `number`是Controller内部唯一pin编号。
- `name`用于描述、映射和诊断。
- `drv_data`可保存每个pin的Controller私有数据。

pin编号应尽量与硬件寄存器布局保持自然对应，避免Controller驱动重复建立复杂转换关系。

### 3.3 pinctrl_desc

`struct pinctrl_desc`描述一个Pin Controller的静态能力：

```c
struct pinctrl_desc
{
    const char *name;
    const struct pinctrl_pin_desc *pins;
    unsigned int npins;
    const struct pinctrl_ops *pctlops;
    const struct pinmux_ops *pmxops;
    const struct pinconf_ops *confops;
    /* Linux还包含module、custom pinconf和consumer link字段。 */
};
```

三个operations集合的职责不同：

| Operations | 职责 |
| --- | --- |
| `pinctrl_ops` | 枚举Group、查询Group中的pin、解析设备树配置节点 |
| `pinmux_ops` | 枚举Function及其可用Group，并执行mux切换 |
| `pinconf_ops` | 读取或设置单pin、Group的电气属性 |

### 3.4 pinctrl_dev

`struct pinctrl_dev`是Controller注册后的运行对象。它持有：

- `pinctrl_desc`。
- Controller私有`driver_data`。
- pin、Group和Function的运行时索引。
- GPIO range。
- pin所有权信息。
- 锁、全局链表、hog state和debugfs信息。

`pinctrl_desc`回答“Controller具备什么能力”，`pinctrl_dev`回答“这个Controller实例当前如何运行”。

### 3.5 pinctrl

`struct pinctrl`是Consumer持有的状态容器。Linux为每个Consumer设备建立独立对象：

```text
struct pinctrl
    -> Consumer device
    -> states
    -> current state
```

这意味着UART2的`default`和I2C0的`default`是两个不同Consumer中的同名状态，选择UART2的`default`不会应用I2C0的设置。

### 3.6 pinctrl_state

`struct pinctrl_state`表示一个命名状态：

```c
struct pinctrl_state
{
    const char *name;
    struct list_head settings;
};
```

状态名称描述生命周期语义，而不是外设名称。常见名称是`default/init/idle/sleep`。

### 3.7 pinctrl_setting

`struct pinctrl_setting`是State中的一个原子设置，主要类型包括：

```text
PIN_MAP_TYPE_MUX_GROUP
PIN_MAP_TYPE_CONFIGS_PIN
PIN_MAP_TYPE_CONFIGS_GROUP
```

Setting持有目标`pinctrl_dev`及mux或config数据。一个State可以跨多个Setting，甚至跨多个Pin Controller。

### 3.8 pinctrl_map

`struct pinctrl_map`是Machine或设备树描述与Core运行对象之间的中间形式。它表达：

```text
哪个Consumer
    -> 哪个State
    -> 使用哪个Controller
    -> 应用哪个Function/Group或Config
```

Linux支持两种主要来源：

1. Board代码使用`pinctrl_register_mappings()`注册静态map。
2. 设备树通过`pinctrl-names`和`pinctrl-N`引用Controller配置节点。

设备树节点的内部格式由具体Controller binding定义。Controller的`dt_node_to_map()`负责将厂商描述转换为通用`pinctrl_map`。

### 3.9 Provider注册流程

Pin Controller驱动准备`pinctrl_desc`和私有数据后，典型流程是：

```text
pinctrl_register_and_init(desc, dev, driver_data, &pctldev)
    -> 分配pinctrl_dev
    -> 关联desc和driver_data
    -> 检查pinctrl/pinmux/pinconf operations
    -> 注册所有pin描述

pinctrl_enable(pctldev)
    -> 应用Controller hog state
    -> 加入全局Controller链表
    -> 建立debugfs节点
```

Linux将“对象初始化”和“对外启用”分开，避免Controller尚未准备完成时Core提前回调驱动。

### 3.10 Device Tree映射流程

Consumer设备树节点通常描述为：

```dts
uart2 {
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&uart2_default>;
    pinctrl-1 = <&uart2_sleep>;
};
```

解析流程是：

```text
pinctrl_dt_to_map()
    -> 枚举pinctrl-0、pinctrl-1...
    -> 从pinctrl-names取得State名称
    -> 查找每个配置节点对应的Pin Controller
    -> 调用Controller dt_node_to_map()
    -> 生成pinctrl_map
    -> 转换为Consumer的state和setting
```

设备树决定Board连接关系，Controller驱动解释寄存器和合法Function，Consumer驱动不包含板级pin编号。

### 3.11 Consumer自动绑定

Linux Driver Core在设备驱动probe前调用：

```text
really_probe()
    -> pinctrl_bind_pins()
        -> devm_pinctrl_get()
        -> pinctrl_lookup_state("default")
        -> pinctrl_lookup_state("init")
        -> 有init时选择init，否则选择default
    -> driver probe
    -> pinctrl_init_done()
        -> 若仍处于init，则切换到default
```

因此，大多数UART、I2C和SPI驱动不需要手工选择`default`状态。只有运行时切换特殊状态时，Consumer驱动才主动调用Pinctrl接口。

### 3.12 State应用顺序

Linux的`pinctrl_select_state()`通过`pinctrl_commit_state()`应用状态：

```text
释放旧State的mux所有权
    -> 应用新State的全部mux setting
    -> 应用新State的全部pinconf setting
    -> 更新current state
```

先mux、后pinconf是明确的Core行为。失败时Core会尽可能解除已应用的新mux并恢复旧State。

### 3.13 Pin所有权与冲突

Linux Pinmux Core跟踪每个pin的mux owner和GPIO owner。当两个Consumer申请重叠pin，或者GPIO和外设功能冲突时，Core可以拒绝后来的申请。

Controller驱动只负责执行合法的硬件切换，不需要重复实现通用所有权管理。Controller仍可通过`pinmux_ops.strict`或私有检查表达额外硬件约束。

### 3.14 与GPIO子系统的关系

Pinctrl与GPIO职责不同：

```text
Pinctrl    负责mux和电气属性
GPIO       负责方向、电平和GPIO中断
```

Linux通过`pinctrl_gpio_range`建立GPIO编号与Controller pin编号的映射。GPIO驱动可以在request、free和direction操作中调用Pinctrl后端，但普通外设驱动不应直接调用这些GPIO专用Pinctrl接口。

### 3.15 Linux模型的代价

Linux完整模型依赖：

- Device Model和Driver Core。
- Device Tree或静态Machine Map。
- 动态内存、链表、radix tree和引用计数。
- mutex、模块生命周期和probe defer。
- pin所有权、GPIO range、Power Management和debugfs。

这些机制适合动态、多Controller、多Consumer的通用操作系统，但不应未经需求分析直接复制到当前Nano工程。

## 4. RT-Thread Nano通用Pinctrl架构

### 4.1 设计目标

Nano Pinctrl保留Linux最有价值的架构边界：

1. Provider、Consumer和Board配置分离。
2. `pinctrl_desc`描述Controller静态能力。
3. `pinctrl_dev`表示Controller运行实例。
4. 每个Consumer拥有独立的State集合。
5. State由多个Setting组成。
6. Core先应用mux，再应用pinconf。
7. Controller私有寄存器知识只存在于Controller驱动和SoC数据中。

同时遵循Nano的资源约束：

1. 静态对象，不使用动态内存。
2. 不建立全局注册表、名称搜索和引用计数。
3. 不依赖设备树、platform bus和Linux Device Model。
4. 不把Pin Controller注册为`rt_device`字符设备。
5. 不为尚未出现的运行时冲突提前维护所有权树。
6. 公共层不增加`rt_`前缀，不依赖RT-Thread内核类型，便于移植到其他RTOS或裸机环境。

### 4.2 目录职责

```text
drivers/pinctrl/
    include/pinctrl.h                 Nano Pinctrl公共数据结构和Consumer API
    pinctrl.c                         State查找与应用
    t22_pinctrl/
        include/t22_pinctrl.h         T22 Controller私有结构
        t22_pinctrl.c                 T22 pinmux和pinconf operations

soc/t22-serdes/t22-deserializer/
    t22_deserializer_pinctrl.h        RX pin/function资源定义
    t22_deserializer_pinctrl.c        RX pin描述和寄存器布局数据

boards/t22-deserializer-evb/
    board_pinctrl.c                   Board启用的Consumer状态和连接关系
```

职责边界：

| 层次 | 职责 |
| --- | --- |
| Pinctrl Core | 按State ID查找状态，按统一顺序分发mux和pinconf |
| T22 Controller Driver | 将pin/function/config转换为T22寄存器操作 |
| SoC Chip Data | 描述pin数量、名称、复用位置、电气寄存器位置和可用功能编号 |
| Board | 选择设备使用哪些pin、function、config和State |
| UART/I2C/SPI/GPIO | 请求自身状态，不访问T22 MFP寄存器 |

### 4.3 pinctrl_pin_desc

```c
struct pinctrl_pin_desc
{
    uint32_t number;
    const char *name;
};
```

该结构对应Linux同名对象，描述Controller拥有的pin编号和名称。当前Core使用`npins`进行边界判断，pin名称保留为Controller元数据。

Nano不建立Linux的动态`pin_desc`运行树，也不在该结构中维护owner和use count。

### 4.4 pinctrl_desc

```c
struct pinctrl_desc
{
    const char *name;
    const struct pinctrl_pin_desc *pins;
    uint32_t npins;
    const struct pinmux_ops *pmxops;
    const struct pinconf_ops *confops;
};
```

`pinctrl_desc`描述一个Controller的静态能力。当前T22需要pinmux和pinconf，因此仅保留这两组operations；尚未实现Function/Group枚举和设备树解析，不增加Linux的`pinctrl_ops`。

### 4.5 pinctrl_dev

```c
struct pinctrl_dev
{
    const struct pinctrl_desc *desc;
    void *driver_data;
};
```

`pinctrl_dev`是Controller运行实例：

- `desc`指向静态能力描述。
- `driver_data`指向具体Controller私有对象。

Nano由静态初始化流程建立该对象，不需要Controller注册、注销和全局链表。

### 4.6 pinmux_ops与pinconf_ops

```c
struct pinmux_ops
{
    int (*set_mux)(struct pinctrl_dev *pctldev,
                   uint32_t pin,
                   uint32_t function);
};

struct pinconf_ops
{
    int (*pin_config_set)(struct pinctrl_dev *pctldev,
                          uint32_t pin,
                          uint32_t config);
};
```

Nano直接使用`pin + function`，没有引入Linux的Function selector和Group selector。该模型适合T22每个MFP独立配置的硬件形式，也减少了名称转换和Group枚举代码。

`config`由Core视为Controller不透明值。当前T22 Board使用T22 Pad寄存器编码，Core不解析其位域。

### 4.7 pinctrl_setting

```c
struct pinctrl_setting
{
    uint32_t pin;
    uint32_t function;
    uint32_t config;
};
```

Nano将Linux中的mux map、config map和运行时setting压缩成一个静态结构。每一项同时描述一个pin的复用值和电气配置。

这种压缩适用于当前“一项Board配置完整描述一个pin”的场景。若未来出现一个Group必须通过单独共享寄存器完成原子切换，才需要增加Group级Setting，而不是现在提前引入。

### 4.8 pinctrl_state

```c
enum pinctrl_state_id
{
    PINCTRL_STATE_DEFAULT = 0,
    PINCTRL_STATE_INIT,
    PINCTRL_STATE_IDLE,
    PINCTRL_STATE_SLEEP
};

struct pinctrl_state
{
    enum pinctrl_state_id id;
    const struct pinctrl_setting *settings;
    size_t nsettings;
};
```

Nano使用枚举代替Linux字符串状态名，避免字符串比较和动态查找，同时保留Linux定义的生命周期语义。

State ID属于Consumer自身的状态集合。UART2和I2C0都可以拥有`PINCTRL_STATE_DEFAULT`，两者不会混合。

### 4.9 pinctrl

```c
struct pinctrl
{
    struct pinctrl_dev *pctldev;
    const struct pinctrl_state *states;
    size_t nstates;
};
```

该对象对应Linux的Consumer handle。一个设备实例定义一个`pinctrl`对象，关联：

```text
该设备由哪个Controller提供引脚
该设备有哪些State
每个State包含哪些Setting
```

当前结构不缓存current state。重复选择同一State会重复写入相同寄存器，行为是幂等的；只有出现明确的性能或写寄存器副作用问题时，才增加状态缓存。

### 4.10 State选择流程

Consumer调用：

```c
pinctrl_select_state(&consumer_pinctrl,
                     PINCTRL_STATE_DEFAULT);
```

Core执行：

```text
在Consumer自己的states中查找State ID
    -> 检查State及Controller基本结构
    -> 遍历所有Setting并调用set_mux()
    -> 再次遍历所有Setting并调用pin_config_set()
```

该顺序与Linux保持一致。Core只进行通用分发，不包含T22寄存器地址、MFP偏移和受保护CSR访问。

### 4.11 静态Board映射

Nano没有设备树，Board代码直接定义Consumer状态：

```c
static const struct pinctrl_setting board_uart2_settings[] =
{
    { .pin = UART2_RX_PIN, .function = UART2_RX_FUNCTION, .config = RX_CONFIG },
    { .pin = UART2_TX_PIN, .function = UART2_TX_FUNCTION, .config = TX_CONFIG }
};

static const struct pinctrl_state board_uart2_states[] =
{
    {
        .id = PINCTRL_STATE_DEFAULT,
        .settings = board_uart2_settings,
        .nsettings = ARRAY_SIZE(board_uart2_settings)
    }
};
```

后续增加I2C0时，应新增独立对象：

```text
board_i2c0_settings
board_i2c0_states
board_i2c0_pinctrl
```

不得把I2C0 Setting放入UART2 State。这样选择UART2的`default`时只配置UART2引脚。

### 4.12 与RT-Thread设备框架的关系

Pinctrl是资源控制框架，不是面向应用的字符设备。它不需要`open/read/write/control`语义，因此不注册为`rt_device`。

RT-Thread UART、I2C和SPI设备在Board实例化或驱动初始化阶段选择Pinctrl State；应用继续通过`rt_device`或对应Bus框架使用外设。

### 4.13 当前有意裁剪的Linux能力

当前Nano不实现：

- `pinctrl_get/put`动态生命周期。
- `pinctrl_lookup_state()`字符串接口。
- 全局Pin Controller注册表。
- `pinctrl_map`动态转换。
- Device Tree和probe defer。
- Function/Group名称枚举。
- pin mux ownership和冲突仲裁。
- GPIO range及GPIO后端接口。
- hog、自动Power Management和debugfs。
- 跨多个Controller的单一State。

这些不是框架缺陷，而是当前静态、单Controller生产系统的明确裁剪。只有出现真实使用场景时才增加对应能力。

## 5. T22 Pinctrl Controller实现

### 5.1 T22硬件职责

T22 Pinctrl涉及三类硬件控制：

1. CSR MFP mode字段：选择每个MFP的复用功能。
2. MISC MFP config寄存器：设置每个MFP的电气属性。
3. CSR/MISC全局控制：允许软件控制MFP，并选择Board需要的软件控制范围。

CSR寄存器需要通过`t22_serdes_csr_update32()`执行受保护更新；普通MISC Pin配置使用直接MMIO写入。

### 5.2 t22_pin_reg_desc

```c
struct t22_pin_reg_desc
{
    uint16_t mux_offset;
    uint16_t config_offset;
    uint8_t mux_shift;
};
```

该结构描述一个MFP对应的硬件布局：

- `mux_offset`：MFP mode寄存器相对CSR base的偏移。
- `mux_shift`：该MFP的4 bit Function字段位置。
- `config_offset`：Pad配置寄存器相对MISC base的偏移。

Controller驱动通过该表完成pin编号到寄存器的转换，不在Board中散落寄存器地址。

### 5.3 t22_pinctrl_soc_data

```c
struct t22_pinctrl_soc_data
{
    struct pinctrl_desc desc;
    const struct t22_pin_reg_desc *pin_reg_descs;
    uintptr_t csr_base;
    uintptr_t misc_base;
    uint16_t mfp_control_offset;
    uint16_t pin_control_offset;
};
```

该对象描述具体T22芯片变体的Pinctrl能力：

- pin总数和pin名称。
- 每个pin的mux/config寄存器布局。
- CSR和MISC基址。
- 软件MFP控制寄存器偏移。
- T22 Controller operations。

Deserializer和Serializer复用同一T22 Controller驱动，但可以提供不同`soc_data`，用于表达pin数量、MFP编号或寄存器布局差异。

### 5.4 t22_pinctrl

```c
struct t22_pinctrl
{
    struct pinctrl_dev pctldev;
    const struct t22_pinctrl_soc_data *soc;
};
```

该对象是T22 Controller运行实例。`pctldev.driver_data`指回`t22_pinctrl`，operations据此取得芯片变体数据。

### 5.5 Controller初始化

`t22_pinctrl_init()`完成：

```text
保存soc_data
    -> 将pctldev.desc关联到soc_data.desc
    -> 设置pctldev.driver_data
    -> 写MISC pin_control范围
    -> 使能CSR的软件MFP控制
```

该接口只初始化Controller能力，不选择UART、I2C或SPI的具体引脚。具体Consumer State由Board决定。

### 5.6 T22 Pinmux操作

`t22_pinmux_set_mux()`执行：

```text
从driver_data取得t22_pinctrl
    -> 通过pin索引取得t22_pin_reg_desc
    -> 计算CSR mux寄存器地址和mask
    -> 使用受保护CSR更新接口写入4 bit Function
```

Function编号属于具体T22芯片能力，由SoC芯片数据或对应芯片头文件定义，Board只选择已确认的合法Function。

### 5.7 T22 Pinconf操作

`t22_pinconf_set()`执行：

```text
通过pin索引取得config_offset
    -> 计算MISC配置寄存器地址
    -> 写入Board选择的Pad配置值
```

当前`config`是T22 Controller不透明编码。后续若需要跨多个Controller复用相同的上拉、驱动能力描述，可以引入通用`pinconf_param + argument`编码，再由T22驱动转换为寄存器值；在只有T22单一Controller时，不提前增加该转换层。

### 5.8 T22 Deserializer芯片数据

当前Deserializer提供17个MFP描述：

```text
MFP0 ... MFP16
```

`t22_deserializer_pin_reg_descs[]`逐pin记录mux offset、mux shift和config offset；`t22_deserializer_pinctrl_data`将该表、pin描述和T22 operations组合为完整SoC数据。

这些信息属于芯片事实，因此位于：

```text
soc/t22-serdes/t22-deserializer/
```

不应移动到EVB Board文件。

### 5.9 T22 Deserializer EVB Board配置

当前Board选择UART2：

| 信号 | Pin | Function | Pad配置 |
| --- | ---: | ---: | ---: |
| UART2 RX | MFP4 | 9 | `BOARD_UART2_RX_PAD_CONFIG` |
| UART2 TX | MFP14 | 2 | `BOARD_UART2_TX_PAD_CONFIG` |

Board定义一个UART2 Consumer对象，只包含`PINCTRL_STATE_DEFAULT`。Early Console和正式UART初始化选择同一个状态，不为Early Console建立特殊状态。

完整调用链：

```text
board_early_init()
    -> board_pinctrl_init()
        -> t22_pinctrl_init()

board_early_console_init()或board_uart_init()
    -> board_uart2_pinctrl_select_state(DEFAULT)
        -> pinctrl_select_state()
            -> t22_pinmux_set_mux(MFP4, function 9)
            -> t22_pinmux_set_mux(MFP14, function 2)
            -> t22_pinconf_set(MFP4, RX config)
            -> t22_pinconf_set(MFP14, TX config)
```

### 5.10 增加新Consumer的方法

以I2C0为例，扩展步骤应是：

1. 在对应芯片数据中确认I2C0可用pin和Function编号。
2. 在Board中定义`board_i2c0_settings[]`。
3. 定义`board_i2c0_states[]`，至少包含`PINCTRL_STATE_DEFAULT`。
4. 定义独立的`board_i2c0_pinctrl`。
5. 在I2C0 Controller初始化前选择其`default`状态。

Pinctrl Core和T22 Controller驱动不应因新增I2C0而修改。

### 5.11 增加Serializer的方法

Serializer若与Deserializer使用相同MFP硬件模型，则只需要增加：

```text
soc/t22-serdes/t22-serializer/t22_serializer_pinctrl.c
soc/t22-serdes/t22-serializer/t22_serializer_pinctrl.h
boards/t22-serializer-evb/board_pinctrl.c
```

Serializer提供自己的pin数量、pin数据和Function常量，继续复用`drivers/pinctrl/t22_pinctrl/`。

## 6. 与Linux模型的对应关系

| Linux对象或机制 | Nano对应实现 | 取舍 |
| --- | --- | --- |
| `pinctrl_pin_desc` | 同名静态结构 | 保留pin编号和名称 |
| `pinctrl_desc` | 同名静态结构 | 保留pins、pmxops、confops |
| `pinctrl_dev` | `desc + driver_data` | 删除动态索引、锁和全局链表 |
| `pinctrl` | Consumer静态State容器 | 删除设备引用和kref |
| `pinctrl_state` | 枚举ID加静态Setting数组 | 枚举代替字符串 |
| `pinctrl_setting` | `pin/function/config` | 合并mux和pinconf map |
| `pinctrl_map` | Board静态对象直接表达 | 不保留中间转换层 |
| Function/Group | 直接pin/function | 适配T22逐pin复用 |
| `pinctrl_select_state()` | 同名Consumer API | 保留先mux后pinconf |
| Device Tree | Board C静态表 | 编译期确定资源关系 |
| 自动probe绑定 | Board初始化显式选择 | 当前无通用platform bus |
| pin owner冲突检测 | 暂无 | Board静态审查保证 |
| GPIO range | 暂无 | GPIO子系统设计时再评估 |

Nano不是复制Linux代码，而是保留其稳定的数据关系和职责边界，再用静态对象替代动态发现机制。

## 7. 当前限制与扩展条件

### 7.1 Pin冲突管理

当前Board配置在编译期固定，Core不维护pin owner。如果后续出现以下任一场景，应增加轻量级所有权机制：

- 多个设备在运行时切换到重叠pin。
- GPIO可以动态申请已经用于UART、I2C或SPI的pin。
- 同一Function支持多组运行时可切换引脚。

在此之前，仅为静态系统增加owner数组会消耗RAM且没有生产收益。

### 7.2 通用Pinconf编码

当前Board配置使用T22原始Pad配置编码。若未来加入第二类Pin Controller，并要求Board用统一语义描述上拉和驱动能力，应增加：

```text
pinconf parameter
pinconf argument
Controller-specific translation
```

该扩展应参考Linux Generic Pinconf，但不需要复制其设备树解析和debugfs机制。

### 7.3 Group级配置

当前Setting逐pin配置。只有硬件要求一组pin必须原子切换，或多个pin共享不可拆分寄存器时，才增加Group对象和Group operations。

### 7.4 Power Management

当系统引入休眠和运行时电源管理后，可为现有Consumer增加`IDLE`和`SLEEP` State，并在设备生命周期中显式选择。State数据模型无需改变。

### 7.5 与GPIO整合

GPIO子系统实现前必须明确：

1. GPIO方向和电平由GPIO Controller负责。
2. GPIO Function mux和电气属性由Pinctrl负责。
3. Board决定某个pin默认作为GPIO还是外设功能。
4. 是否需要动态GPIO申请和Pinctrl所有权仲裁，应由真实应用场景决定。

不得让GPIO驱动重新实现一套MFP寄存器写入流程。

## 8. 生产代码约束

1. Consumer不得包含T22 MFP寄存器地址和位域。
2. Board不得直接执行CSR/MISC寄存器读写。
3. SoC芯片数据不得包含EVB连接策略。
4. T22 Controller驱动不得硬编码UART2、I2C0等Board选择。
5. 新设备应通过新增Consumer状态表完成扩展，不修改Core分发流程。
6. 不为测试统计增加全局状态、计数器或专用分支。
7. 不为未出现的动态场景提前加入链表、动态内存和所有权管理。
8. 通用接口命名优先对齐Linux；RT-Thread已有标准接口时优先复用RT-Thread。

## 9. 验证标准

Pinctrl代码或数据修改后至少验证：

1. Debug和Release构建通过。
2. Early Console能够正常输出。
3. 正式UART设备能够接管控制台。
4. UART2 RX/TX对应MFP Function正确。
5. Pad电气配置与Board设计一致。
6. RT-Thread Demo双线程持续运行。
7. 新增Consumer只影响自身State中的pin。
8. Serializer或其他芯片变体不需要复制T22通用Controller驱动。

## 10. 设计结论

Linux Pinctrl的核心价值不在设备树或动态对象本身，而在以下分层：

```text
pinctrl_desc / pinctrl_dev
    描述Pin Controller能力和实例

pinctrl / pinctrl_state / pinctrl_setting
    描述Consumer需要的引脚状态

Pinctrl Core
    连接State与Controller operations

Board或固件描述
    决定具体设备使用哪些pin
```

Nano Pinctrl保留了这组关系，并用静态数组、枚举State和逐pin Setting替代Linux的动态map、设备树和全局资源管理。T22实现进一步将通用Controller流程、Deserializer芯片数据和EVB Board选择分离，使后续增加I2C、SPI、GPIO或Serializer时可以沿现有模板扩展，而不需要改写Pinctrl Core。
