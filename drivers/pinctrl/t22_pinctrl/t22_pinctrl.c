#include <errno.h>
#include <stdint.h>

#include "t22_pinctrl.h"
#include "t22_serdes.h"

#define T22_PINCTRL_FUNCTION_MASK       0xFU
#define T22_PINCTRL_SOFTWARE_CONTROL    (1U << 0)

static volatile uint32_t *t22_pinctrl_reg(uintptr_t base,
                                          uint16_t offset)
{
    return (volatile uint32_t *)(base + offset);
}

static int t22_pinmux_set_mux(struct pinctrl_dev *pctldev,
                              uint32_t pin,
                              uint32_t function)
{
    struct t22_pinctrl *pinctrl = pctldev->driver_data;
    const struct t22_pinctrl_soc_data *soc = pinctrl->soc;
    const struct t22_pin_reg_desc *reg_desc = &soc->pin_reg_descs[pin];
    volatile uint32_t *reg;

    if (function > T22_PINCTRL_FUNCTION_MASK)
    {
        return -EINVAL;
    }

    reg = t22_pinctrl_reg(soc->csr_base, reg_desc->mux_offset);
    t22_serdes_csr_update32(reg,
                            T22_PINCTRL_FUNCTION_MASK <<
                            reg_desc->mux_shift,
                            function << reg_desc->mux_shift);
    return 0;
}

static int t22_pinconf_set(struct pinctrl_dev *pctldev,
                           uint32_t pin,
                           uint32_t config)
{
    struct t22_pinctrl *pinctrl = pctldev->driver_data;
    const struct t22_pinctrl_soc_data *soc = pinctrl->soc;
    const struct t22_pin_reg_desc *reg_desc = &soc->pin_reg_descs[pin];

    *t22_pinctrl_reg(soc->misc_base, reg_desc->config_offset) = config;
    return 0;
}

const struct pinmux_ops t22_pinmux_ops =
{
    .set_mux = t22_pinmux_set_mux
};

const struct pinconf_ops t22_pinconf_ops =
{
    .pin_config_set = t22_pinconf_set
};

void t22_pinctrl_init(struct t22_pinctrl *pinctrl,
                      const struct t22_pinctrl_soc_data *soc,
                      uint32_t pin_control_mask)
{
    pinctrl->soc = soc;
    pinctrl->pctldev.desc = &soc->desc;
    pinctrl->pctldev.driver_data = pinctrl;

    *t22_pinctrl_reg(soc->misc_base, soc->pin_control_offset) =
        pin_control_mask;
    t22_serdes_csr_update32(
        t22_pinctrl_reg(soc->csr_base, soc->mfp_control_offset),
        T22_PINCTRL_SOFTWARE_CONTROL,
        T22_PINCTRL_SOFTWARE_CONTROL);
}
