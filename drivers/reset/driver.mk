RESET_DIR := drivers/reset
T22_RESET_DIR := $(RESET_DIR)/t22_reset

SRCS += $(RESET_DIR)/reset.c \
        $(T22_RESET_DIR)/t22_reset.c
INCLUDES += $(RESET_DIR)/include \
            $(T22_RESET_DIR)/include
