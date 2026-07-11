CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
SIZE    := $(CROSS_COMPILE)size

ifeq ($(CPU),e902m)
CPU_FLAGS := -mcpu=e902m -mabi=ilp32e -mcmodel=medlow
else ifeq ($(CPU),e902)
CPU_FLAGS := -mcpu=e902 -mabi=ilp32e -mcmodel=medlow
else
$(error Unsupported CPU: $(CPU))
endif

CPPFLAGS := $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES))
CFLAGS   := $(CPU_FLAGS) -std=gnu11 -ffreestanding -fno-builtin -fno-common \
            -ffunction-sections -fdata-sections -MMD -MP -Wall -Wextra
ASFLAGS  := $(CPU_FLAGS) -ffreestanding -MMD -MP
LDFLAGS  = $(CPU_FLAGS) -nostartfiles -nostdlib -static \
            -Wl,--gc-sections -Wl,-Map,$(MAP_FILE) -T$(LINKER_SCRIPT)
LDLIBS   += -lgcc

ifeq ($(BUILD),debug)
CFLAGS  += -Og -g3
ASFLAGS += -g3
else ifeq ($(BUILD),release)
CFLAGS  += -Os -g1
ASFLAGS += -g1
else
$(error Unsupported build type: $(BUILD))
endif
