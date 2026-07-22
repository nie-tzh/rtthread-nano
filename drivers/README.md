# 驱动目录

`drivers/`按照硬件功能组织可复用驱动，并区分底层硬件访问与RT-Thread设备适配。底层驱动负责控制器操作，适配层向RT-Thread提供统一设备接口。

新增接口前必须先检查RT-Thread内核、组件和已有CPU port。能够复用`rt_irq_desc`、`rt_device`或官方设备子系统时，不定义功能重叠的数据结构和注册机制。寄存器级控制器驱动可以独立存在，但不能演变成与RT-Thread并行的应用设备模型。

基地址、时钟等芯片资源由SoC层提供，外设实例和板载连接由Board层选择。通用驱动不包含具体SoC或板卡头文件。

仅在存在真实实现时创建对应子目录。每个驱动通过`.mk`文件声明源码、头文件路径和宏定义：

```makefile
SRCS     += drivers/serial/dw_apb_uart/dw_apb_uart.c
INCLUDES += drivers/serial/dw_apb_uart/include
```

板卡在`board.mk`中选择需要的驱动模块：

```makefile
DRIVER_MKS += drivers/serial/dw_apb_uart/driver.mk
```

设备适配层可以使用RT-Thread API。应用只能使用驱动接口或RT-Thread Device API，不能直接访问外设寄存器。
