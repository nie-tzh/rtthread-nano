#ifndef PINCTRL_H
#define PINCTRL_H

#include <stddef.h>
#include <stdint.h>

struct pinctrl_dev;

struct pinctrl_pin_desc
{
    uint32_t number;
    const char *name;
};

struct pinctrl_setting
{
    uint32_t pin;
    uint32_t function;
    uint32_t config;
};

enum pinctrl_state_id
{
    PINCTRL_STATE_DEFAULT = 0,
    PINCTRL_STATE_INIT,
    PINCTRL_STATE_IDLE,
    PINCTRL_STATE_SLEEP
};

struct pinctrl_state
{
    enum pinctrl_state_id id;
    const struct pinctrl_setting *settings;
    size_t nsettings;
};

struct pinctrl
{
    struct pinctrl_dev *pctldev;
    const struct pinctrl_state *states;
    size_t nstates;
};

struct pinmux_ops
{
    int (*set_mux)(struct pinctrl_dev *pctldev,
                   uint32_t pin,
                   uint32_t function);
};

struct pinconf_ops
{
    int (*pin_config_set)(struct pinctrl_dev *pctldev,
                          uint32_t pin,
                          uint32_t config);
};

struct pinctrl_desc
{
    const char *name;
    const struct pinctrl_pin_desc *pins;
    uint32_t npins;
    const struct pinmux_ops *pmxops;
    const struct pinconf_ops *confops;
};

struct pinctrl_dev
{
    const struct pinctrl_desc *desc;
    void *driver_data;
};

int pinctrl_select_state(const struct pinctrl *pinctrl,
                         enum pinctrl_state_id state_id);

#endif
