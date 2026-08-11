# 工程架构

## 目标

工程面向T22 SerDes系列芯片及其板卡，保持RT-Thread内核、CPU移植、芯片集成、设备驱动、板级配置和应用之间的单向依赖。目录按硬件事实的归属组织，不按当前调用位置堆放代码。

## 构建身份

默认目标的身份关系为：

```text
BOARD=t22-deserializer-evb
  -> SOC=t22-serdes
  -> CHIP=t22-deserializer

SOC=t22-serdes
  -> CPU=e902
```

- `BOARD`表示可构建的物理板卡和产品目标。
- `SOC`表示两颗芯片共享的T22 SerDes系统集成平台。
- `CHIP`表示具体芯片型号，当前为解串器，后续增加加串器。
- `CPU`表示处理器核，由SoC绑定，当前固定为E902。

使用者通常只选择`BOARD`。构建系统负责推导并校验其余身份，拒绝不匹配的Board、SoC、CHIP和CPU组合。

Board、SoC和CPU的内部绑定不可由命令行覆盖。构建目录保存实际编译和链接参数签名；工具链、参数或参与构建的`.mk`文件变化时，对象文件会自动重新生成。

## 目录职责

```text
apps/                              产品应用和示例
boards/<board>/                    板载连接、资源选择和可选启动覆盖
drivers/                           可复用控制器驱动及RT-Thread设备适配
drivers/interrupt/riscv_clic/      通用RISC-V CLIC寄存器驱动
drivers/pinctrl/                   Pinctrl Core和芯片Controller驱动
drivers/reset/                     Reset Core和芯片Controller驱动
mk/                                工具链和通用构建规则
rt-thread/src/                     RT-Thread内核
rt-thread/components/              RT-Thread官方组件
rt-thread/libcpu/risc-v/e902/      E902异常、上下文和线程栈移植
soc/t22-serdes/                    公共启动、内存布局、系统集成和资源描述
```

依赖方向保持单向：

```text
apps -> RT-Thread API / public device API
boards -> SoC resource API
boards -> driver configuration -> drivers
soc -> CPU architecture API
CPU architecture adapter -> interrupt-controller driver
drivers -> hardware
```

通用驱动不能包含具体Board头文件，应用不能直接访问MMIO寄存器。

## 框架优先原则

实现内核适配或设备驱动前，必须先检查当前RT-Thread内核、`components/`和已有CPU port是否已经提供对应的数据结构、公共接口和生命周期。已有框架能够表达需求时，直接复用并补齐硬件适配，不再建立平行模型。例如IRQ处理表使用`struct rt_irq_desc`，设备注册使用`struct rt_device`和`rt_device_*`接口。

框架复用与底层驱动分工如下：

- RT-Thread已经提供对象模型时，项目代码只实现硬件操作和适配回调，不重新定义同类对象、注册表或应用接口。
- RT-Thread Nano当前源码未携带某个完整设备子系统时，先评估引入RT-Thread官方实现；只有官方模型确实不适用时，才建立项目公共层，并记录缺失能力和边界。
- CLIC、DW APB UART和DW APB Timer等寄存器级IP驱动负责硬件访问，不等同于新的操作系统设备模型；正式运行期接口仍应通过RT-Thread设备框架提供。
- 早期控制台和异常停机输出发生在设备框架建立之前，可以保留最小轮询通道；正式UART设备就绪后由标准控制台接管，应用不继续依赖早期接口。
- `rt_timer`是由系统Tick驱动的软件定时器，不能替代DW APB Timer寄存器驱动；若硬件Timer需要面向应用开放，应再适配RT-Thread硬件Timer或`rt_device`模型。

## 资源归属

同一硬件信息只保留一个来源：

| 信息 | 归属 | 示例 |
| --- | --- | --- |
| CPU架构机制 | `libcpu` | RISC-V CSR、异常入口、`mtvt/mcause`适配、上下文切换 |
| 芯片系统集成 | `soc` | 地址空间、中断号、时钟、复位、T22 MMIO CSR写保护 |
| 控制器操作方法 | `drivers` | RISC-V CLIC、DW UART、DW I2C、DW APB Timer寄存器流程 |
| 芯片引脚控制能力 | `drivers/pinctrl` + `soc` | MFP编码、MISC偏移、ECO规则、引脚数量 |
| 外设复位控制语义 | `drivers/reset` + `soc` | Consumer复位接口、复位ID、复位寄存器资源 |
| 板卡资源选择 | `boards` | UART2作为控制台、TIMER1作为系统Tick时基 |
| 板载连接 | `boards` | 引脚用途、器件地址、默认波特率 |

Board相当于静态硬件配置清单：它选择芯片已经具备的资源，但不重新定义芯片能力。例如芯片提供三路I2C，Board可以只启用其中两路；三路I2C的基地址和中断属于SoC资源，选择哪两路属于Board。

## 芯片差异

解串器和加串器共享T22 SerDes的构建模型和公共接口，但允许资源拓扑不同。差异通过编译期选择的芯片资源数据表达，不复制公共流程：

- 相同IP、不同基地址：分别记录资源地址。
- 相同IP、不同实例数量：分别记录实例表和数量。
- 相同控制器、不同引脚能力：由pinctrl驱动选择对应芯片数据。
- 控制器操作一致：复用同一个通用驱动。
- 寄存器语义确有差异：在驱动中使用芯片描述数据或操作接口隔离。

例如解串器的APHY可以位于`0x00200000`并具有4路MIPI MAC，加串器的APHY可以位于`0x00240000`并只有1路MIPI MAC。后续由SoC以统一方式向Board提供资源，Board只选择当前芯片真实存在且板卡需要启用的实例。

## SoC边界

`soc/t22-serdes/`不是所有外设代码的汇总目录，也不应只是函数转发层。它只保存跨设备、跨板卡仍然成立的芯片集成事实和系统级操作：

- 公共及芯片相关的资源描述，包括基地址、中断、时钟和复位关系。
- T22 MMIO CSR等全局系统控制模块的受保护访问。
- CPU、总线和外设之间的系统级初始化关系。
- 多个外设实例汇聚到同一SoC中断时的注册、分发和资源仲裁。
- 芯片型号、版本和eFuse等系统信息的基础访问。

UART、I2C、GPIO和pinctrl等单一控制器的寄存器流程属于`drivers/`。具体板卡启用哪些实例、采用哪些引脚状态以及连接哪些器件属于`boards/`。

SoC和单一控制器的MMIO寄存器映射均为实现私有数据结构：连续、布局稳定的寄存器块使用`volatile`成员、保留区和必要的`offsetof`/`sizeof`编译期检查；业务接口不传递裸寄存器偏移。跨设备的CSR写保护仍由SoC层封装，单通道UART和DW Timer的寄存器流程仍由对应Driver负责。

## 扩展原则

- 只在出现真实实现时创建目录和接口，不提前建立空框架。
- 两颗芯片先共享接口和流程，只有经过确认的硬件差异才进入芯片数据。
- 两颗芯片共享的启动程序和XIP内存布局由SoC提供默认实现；启动介质或内存布局不同的Board可以通过`STARTUP_SOURCE`和`LINKER_SCRIPT`显式覆盖。
- 后续pinctrl、GPIO、I2C等子系统按控制器功能放入`drivers/`，Board仅提供配置。
- 项目公共服务在出现首个真实模块后再建立独立目录。
