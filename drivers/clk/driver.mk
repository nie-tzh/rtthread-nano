CLK_DIR := drivers/clk
T22_CLK_DIR := $(CLK_DIR)/t22_clk

SRCS += $(CLK_DIR)/clk.c \
        $(CLK_DIR)/clk-fixed-rate.c \
        $(T22_CLK_DIR)/t22_clk.c
INCLUDES += $(CLK_DIR)/include \
            $(T22_CLK_DIR)/include
