.DEFAULT_GOAL := all

APP        ?= demo
BOARD      ?= t22-deserializer-evb
BUILD      ?= debug
TARGET     ?= firmware
O          ?= build/$(BOARD)/$(APP)/$(BUILD)
V          ?= 0

CROSS_COMPILE ?= riscv64-unknown-elf-

BUILD_DIR := $(abspath $(O))
BOARD_DIR := boards/$(BOARD)
APP_DIR   := apps/$(APP)
BOARD_MK  := $(BOARD_DIR)/board.mk
APP_MK    := $(APP_DIR)/app.mk

ifeq ($(wildcard $(BOARD_MK)),)
$(error Unsupported board: $(BOARD))
endif

ifeq ($(wildcard $(APP_MK)),)
$(error Unsupported app: $(APP))
endif

SRCS     :=
INCLUDES :=
DEFINES  :=
LDLIBS   :=
DRIVER_MKS :=

include $(BOARD_MK)

ifeq ($(strip $(BOARD_SOC)),)
$(error Board $(BOARD) does not define BOARD_SOC)
endif

ifeq ($(strip $(BOARD_CHIP)),)
$(error Board $(BOARD) does not define BOARD_CHIP)
endif

ifneq ($(strip $(SOC)),)
ifneq ($(SOC),$(BOARD_SOC))
$(error BOARD=$(BOARD) requires SOC=$(BOARD_SOC), not SOC=$(SOC))
endif
endif

ifneq ($(strip $(CHIP)),)
ifneq ($(CHIP),$(BOARD_CHIP))
$(error BOARD=$(BOARD) requires CHIP=$(BOARD_CHIP), not CHIP=$(CHIP))
endif
endif

SOC      := $(BOARD_SOC)
CHIP     := $(BOARD_CHIP)
SOC_DIR  := soc/$(SOC)
SOC_MK   := $(SOC_DIR)/soc.mk

ifeq ($(wildcard $(SOC_MK)),)
$(error Unsupported SoC: $(SOC))
endif

include $(SOC_MK)

ifeq ($(strip $(SOC_CPU)),)
$(error SoC $(SOC) does not define SOC_CPU)
endif

ifneq ($(strip $(CPU)),)
ifneq ($(CPU),$(SOC_CPU))
$(error SOC=$(SOC) requires CPU=$(SOC_CPU), not CPU=$(CPU))
endif
endif

CPU := $(SOC_CPU)

ifneq ($(strip $(DRIVER_MKS)),)
include $(DRIVER_MKS)
endif

include $(APP_MK)

ifeq ($(strip $(STARTUP_SOURCE)),)
$(error No startup source configured for BOARD=$(BOARD))
endif

ifeq ($(wildcard $(STARTUP_SOURCE)),)
$(error Startup source not found: $(STARTUP_SOURCE))
endif

ifeq ($(strip $(LINKER_SCRIPT)),)
$(error No linker script configured for BOARD=$(BOARD))
endif

ifeq ($(wildcard $(LINKER_SCRIPT)),)
$(error Linker script not found: $(LINKER_SCRIPT))
endif

include mk/toolchain.mk
include mk/rules.mk
