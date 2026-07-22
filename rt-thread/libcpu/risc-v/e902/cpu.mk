E902_CPU_DIR := rt-thread/libcpu/risc-v/e902

SRCS += $(E902_CPU_DIR)/e902_irq.c \
        $(E902_CPU_DIR)/cpuport.c \
        $(E902_CPU_DIR)/cpuport_gcc.S

INCLUDES += rt-thread/include \
            $(E902_CPU_DIR)

DRIVER_MKS += drivers/interrupt/riscv_clic/driver.mk
