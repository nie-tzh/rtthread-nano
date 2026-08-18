override BOARD_SOC  := t22-serdes
override BOARD_CHIP := t22-deserializer

SRCS += $(BOARD_DIR)/board.c \
        $(BOARD_DIR)/board_pinctrl.c \
        $(BOARD_DIR)/board_reset.c
INCLUDES += $(BOARD_DIR)/include

DRIVER_MKS += drivers/serial/dw_apb_uart/driver.mk
DRIVER_MKS += drivers/clk/driver.mk
DRIVER_MKS += drivers/pinctrl/driver.mk
DRIVER_MKS += drivers/reset/driver.mk
