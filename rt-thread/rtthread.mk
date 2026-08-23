RTTHREAD_MK := rt-thread/rtthread.mk
RTTHREAD_CONFIG := $(BOARD_DIR)/include/rtconfig.h
RTTHREAD_CONFIG_DEFINES := $(shell sed -n \
    's/^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\([A-Za-z_][A-Za-z0-9_]*\).*/\1/p' \
    $(RTTHREAD_CONFIG))

INCLUDES += rt-thread/include

SRCS += rt-thread/src/clock.c \
        rt-thread/src/device.c \
        rt-thread/src/idle.c \
        rt-thread/src/irq.c \
        rt-thread/src/kservice.c \
        rt-thread/src/object.c \
        rt-thread/src/scheduler.c \
        rt-thread/src/thread.c \
        rt-thread/src/timer.c \
        $(BOARD_DIR)/rtthread_irq.c

ifneq ($(filter RT_USING_COMPONENTS_INIT,$(RTTHREAD_CONFIG_DEFINES)),)
SRCS += rt-thread/src/components.c
endif

ifneq ($(filter RT_USING_SEMAPHORE,$(RTTHREAD_CONFIG_DEFINES)),)
SRCS += rt-thread/src/ipc.c
endif

ifneq ($(filter RT_USING_FINSH,$(RTTHREAD_CONFIG_DEFINES)),)
INCLUDES += rt-thread/components/finsh
SRCS += rt-thread/components/finsh/cmd.c \
        rt-thread/components/finsh/msh.c \
        rt-thread/components/finsh/msh_file.c \
        rt-thread/components/finsh/msh_parse.c \
        rt-thread/components/finsh/shell.c
LDLIBS += -lc
endif

# RT-Thread v4.1.1 has configuration-dependent warnings in these objects.
$(BUILD_DIR)/obj/rt-thread/src/idle.o: CFLAGS += -Wno-unused-parameter
$(BUILD_DIR)/obj/rt-thread/src/kservice.o: CFLAGS += \
    -Wno-implicit-fallthrough -Wno-unused-parameter
$(BUILD_DIR)/obj/rt-thread/src/thread.o: CFLAGS += \
    -Wno-unused-parameter -Wno-maybe-uninitialized
