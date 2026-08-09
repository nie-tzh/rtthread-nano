override SOC_CPU := e902

ifeq ($(filter $(CHIP),t22-deserializer),)
$(error Unsupported CHIP=$(CHIP) for SOC=$(SOC))
endif

CHIP_DIR := $(SOC_DIR)/$(CHIP)
CHIP_MK  := $(CHIP_DIR)/chip.mk

ifeq ($(wildcard $(CHIP_MK)),)
$(error Unsupported CHIP=$(CHIP) for SOC=$(SOC))
endif

STARTUP_SOURCE ?= $(SOC_DIR)/startup.S
SRCS += $(STARTUP_SOURCE) \
        $(SOC_DIR)/interrupt_vectors.S \
        $(SOC_DIR)/t22_serdes.c \
        $(SOC_DIR)/t22_serdes_irq.c \
        $(SOC_DIR)/t22_serdes_timer.c
INCLUDES += $(SOC_DIR)/include

include $(CHIP_MK)

DRIVER_MKS += drivers/timer/dw_apb_timer/driver.mk

LINKER_SCRIPT ?= $(SOC_DIR)/linker.ld
