# T22 SerDes SoC层

本目录描述T22解串器和加串器共享的系统集成平台。当前提供公共启动程序、默认XIP内存布局、公共地址、目标时钟频率、T22 MMIO CSR受保护写和外设资源描述，并将T22 SerDes绑定到E902 CPU；后续资源描述按具体芯片在编译期选择。PLL配置和Clock Provider位于`drivers/clk/t22_clk/`。

T22层持有具体CLIC实例、硬件向量表和`desc`表，并通过`struct riscv_clic_config`提供CLIC基地址、T22的`MINTTHRESH`地址及`CLICINFO.num_interrupt=0`兼容值。通用CLIC驱动不包含这些SoC事实，E902 CPU层也不固定T22地址。

当前T22公共资源地址表如下。表中地址描述SoC集成事实；具体芯片或Board是否启用某个控制器，由对应配置决定。

| 模块 | 基地址 | 模块 | 基地址 |
| --- | --- | --- | --- |
| I2C0 | `0x00100000` | I2C1 | `0x00100400` |
| I2C2 | `0x00100800` | CSRAO | `0x00100C00` |
| SPI1_M | `0x00101000` | SPI1_S | `0x00101400` |
| UART0 | `0x00101800` | UART1 | `0x00101C00` |
| UART2 | `0x00102000` | SPI_M | `0x00102400` |
| SPI_S | `0x00102800` | GPIO | `0x00102C00` |
| CSR | `0x00103000` | TIMER | `0x00103400` |
| WDT | `0x00103800` | EFUSE | `0x00103C00` |
| SRAM | `0x00140000` | ROM | `0x00180000` |
| MISC | `0x0025C000` | CLIC | `0xE0800000` |

单一控制器的寄存器操作属于`drivers/`，板卡启用的设备实例和引脚用途属于`boards/`。SoC层不复制驱动流程，也不保存板级连接策略。UART和Timer等Consumer通过Clock Core查询输入频率，不直接依赖SoC频率常量。

SoC系统级寄存器映射封装在实现文件的私有类型中，使用`volatile`成员、保留区和必要的偏移静态检查；公共SoC接口只表达系统操作，不向Board或应用暴露裸寄存器地址。

Board默认复用本目录的启动程序和链接脚本。只有启动介质或内存布局确实不同的板卡，才在自身配置中覆盖`STARTUP_SOURCE`和`LINKER_SCRIPT`。

当前Clock Provider固定采用eFuse=0对应的生产配置：AHB 320 MHz、APB 200 MHz、SPI 200 MHz，不读取eFuse寄存器，也不保留运行时分支。
