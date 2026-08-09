PINCTRL_DIR := drivers/pinctrl
T22_PINCTRL_DIR := $(PINCTRL_DIR)/t22_pinctrl

SRCS += $(PINCTRL_DIR)/pinctrl.c \
        $(T22_PINCTRL_DIR)/t22_pinctrl.c
INCLUDES += $(PINCTRL_DIR)/include \
            $(T22_PINCTRL_DIR)/include
