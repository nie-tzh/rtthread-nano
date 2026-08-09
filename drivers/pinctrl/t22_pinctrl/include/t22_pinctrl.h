#ifndef T22_PINCTRL_H
#define T22_PINCTRL_H

#include <stdint.h>

#include "pinctrl.h"

struct t22_pin_reg_desc
{
    uint16_t mux_offset;
    uint16_t config_offset;
    uint8_t mux_shift;
};

struct t22_pinctrl_soc_data
{
    struct pinctrl_desc desc;
    const struct t22_pin_reg_desc *pin_reg_descs;
    uintptr_t csr_base;
    uintptr_t misc_base;
    uint16_t mfp_control_offset;
    uint16_t pin_control_offset;
};

struct t22_pinctrl
{
    struct pinctrl_dev pctldev;
    const struct t22_pinctrl_soc_data *soc;
};

extern const struct pinmux_ops t22_pinmux_ops;
extern const struct pinconf_ops t22_pinconf_ops;

void t22_pinctrl_init(struct t22_pinctrl *pinctrl,
                      const struct t22_pinctrl_soc_data *soc,
                      uint32_t pin_control_mask);

#endif
