# 驱动目录

`drivers/`按照硬件功能组织可复用驱动，并区分底层硬件访问与RT-Thread设备适配。底层驱动负责控制器操作，适配层向RT-Thread提供统一设备接口。

仅在存在真实实现时创建对应子目录。每个驱动通过`.mk`文件声明源码、头文件路径和宏定义：

```makefile
SRCS     += drivers/<class>/<driver>/<source>.c
INCLUDES += drivers/<class>/<driver>/include
```

板卡在`board.mk`中选择需要的驱动模块：

```makefile
DRIVER_MKS += drivers/<class>/<driver>/driver.mk
```

设备适配层可以使用RT-Thread API。应用只能使用驱动接口或RT-Thread Device API，不能直接访问外设寄存器。
