E902_CPU_DIR := rt-thread/libcpu/risc-v/e902

SRCS += $(E902_CPU_DIR)/clic.c \
        $(E902_CPU_DIR)/exception.c \
        $(E902_CPU_DIR)/exception_gcc.S \
        $(E902_CPU_DIR)/interrupt_gcc.S

INCLUDES += $(E902_CPU_DIR)/include
