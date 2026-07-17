override SOC_CPU := e902

ifeq ($(filter $(CHIP),t22-deserializer),)
$(error Unsupported CHIP=$(CHIP) for SOC=$(SOC))
endif

STARTUP_SOURCE ?= $(SOC_DIR)/startup.S
SRCS += $(STARTUP_SOURCE) \
        $(SOC_DIR)/interrupt_vectors.S \
        $(SOC_DIR)/t22_serdes.c \
        $(SOC_DIR)/t22_serdes_irq.c
INCLUDES += $(SOC_DIR)/include

LINKER_SCRIPT ?= $(SOC_DIR)/linker.ld
