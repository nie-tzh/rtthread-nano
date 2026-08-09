#include "t22_deserializer_pinctrl.h"
#include "t22_serdes.h"

#define T22_DESERIALIZER_MFP_CONTROL_OFFSET    0x0D4U
#define T22_DESERIALIZER_PIN_CONTROL_OFFSET    0x0B8U

static const struct pinctrl_pin_desc
    t22_deserializer_pin_descs[T22_DESERIALIZER_PIN_COUNT] =
{
    [0]  = { .number = 0U,  .name = "MFP0" },
    [1]  = { .number = 1U,  .name = "MFP1" },
    [2]  = { .number = 2U,  .name = "MFP2" },
    [3]  = { .number = 3U,  .name = "MFP3" },
    [4]  = { .number = 4U,  .name = "MFP4" },
    [5]  = { .number = 5U,  .name = "MFP5" },
    [6]  = { .number = 6U,  .name = "MFP6" },
    [7]  = { .number = 7U,  .name = "MFP7" },
    [8]  = { .number = 8U,  .name = "MFP8" },
    [9]  = { .number = 9U,  .name = "MFP9" },
    [10] = { .number = 10U, .name = "MFP10" },
    [11] = { .number = 11U, .name = "MFP11" },
    [12] = { .number = 12U, .name = "MFP12" },
    [13] = { .number = 13U, .name = "MFP13" },
    [14] = { .number = 14U, .name = "MFP14" },
    [15] = { .number = 15U, .name = "MFP15" },
    [16] = { .number = 16U, .name = "MFP16" }
};

static const struct t22_pin_reg_desc
    t22_deserializer_pin_reg_descs[T22_DESERIALIZER_PIN_COUNT] =
{
    [0]  = { .mux_offset = 0x0D8U, .config_offset = 0x028U, .mux_shift = 28U },
    [1]  = { .mux_offset = 0x0D8U, .config_offset = 0x030U, .mux_shift = 24U },
    [2]  = { .mux_offset = 0x0D8U, .config_offset = 0x034U, .mux_shift = 20U },
    [3]  = { .mux_offset = 0x0D8U, .config_offset = 0x038U, .mux_shift = 16U },
    [4]  = { .mux_offset = 0x0D8U, .config_offset = 0x03CU, .mux_shift = 12U },
    [5]  = { .mux_offset = 0x0D8U, .config_offset = 0x040U, .mux_shift = 8U },
    [6]  = { .mux_offset = 0x0D8U, .config_offset = 0x044U, .mux_shift = 4U },
    [7]  = { .mux_offset = 0x0D8U, .config_offset = 0x048U, .mux_shift = 0U },
    [8]  = { .mux_offset = 0x0DCU, .config_offset = 0x04CU, .mux_shift = 28U },
    [9]  = { .mux_offset = 0x0DCU, .config_offset = 0x050U, .mux_shift = 24U },
    [10] = { .mux_offset = 0x0DCU, .config_offset = 0x054U, .mux_shift = 20U },
    [11] = { .mux_offset = 0x0DCU, .config_offset = 0x058U, .mux_shift = 16U },
    [12] = { .mux_offset = 0x0DCU, .config_offset = 0x05CU, .mux_shift = 12U },
    [13] = { .mux_offset = 0x0DCU, .config_offset = 0x060U, .mux_shift = 8U },
    [14] = { .mux_offset = 0x0DCU, .config_offset = 0x064U, .mux_shift = 4U },
    [15] = { .mux_offset = 0x0DCU, .config_offset = 0x068U, .mux_shift = 0U },
    [16] = { .mux_offset = 0x0E0U, .config_offset = 0x06CU, .mux_shift = 0U }
};

const struct t22_pinctrl_soc_data t22_deserializer_pinctrl_data =
{
    .desc =
    {
        .name = "t22-deserializer-pinctrl",
        .pins = t22_deserializer_pin_descs,
        .npins = T22_DESERIALIZER_PIN_COUNT,
        .pmxops = &t22_pinmux_ops,
        .confops = &t22_pinconf_ops
    },
    .pin_reg_descs = t22_deserializer_pin_reg_descs,
    .csr_base = T22_SERDES_CSR_BASE,
    .misc_base = T22_SERDES_MISC_BASE,
    .mfp_control_offset = T22_DESERIALIZER_MFP_CONTROL_OFFSET,
    .pin_control_offset = T22_DESERIALIZER_PIN_CONTROL_OFFSET
};
