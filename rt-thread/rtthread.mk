RTTHREAD_MK := rt-thread/rtthread.mk
RTTHREAD_CONFIG := $(BOARD_DIR)/include/rtconfig.h

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

# RT-Thread v4.1.1 has configuration-dependent warnings in these objects.
$(BUILD_DIR)/obj/rt-thread/src/idle.o: CFLAGS += -Wno-unused-parameter
$(BUILD_DIR)/obj/rt-thread/src/kservice.o: CFLAGS += \
    -Wno-implicit-fallthrough -Wno-unused-parameter
$(BUILD_DIR)/obj/rt-thread/src/thread.o: CFLAGS += \
    -Wno-unused-parameter -Wno-maybe-uninitialized
