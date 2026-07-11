override SOC_CPU := e902

STARTUP_SOURCE ?= $(SOC_DIR)/startup.S
SRCS += $(STARTUP_SOURCE)

LINKER_SCRIPT ?= $(SOC_DIR)/linker.ld
