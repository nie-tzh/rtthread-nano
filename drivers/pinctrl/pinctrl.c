#include <errno.h>

#include "pinctrl.h"

static int pinctrl_apply_state(struct pinctrl_dev *pctldev,
                               const struct pinctrl_state *state)
{
    const struct pinctrl_desc *desc;
    const struct pinctrl_setting *setting;
    int result;
    size_t index;

    if ((pctldev == NULL) || (pctldev->desc == NULL) ||
        (state == NULL))
    {
        return -EINVAL;
    }

    desc = pctldev->desc;
    if ((desc->pins == NULL) || (desc->pmxops == NULL) ||
        (desc->pmxops->set_mux == NULL) || (desc->confops == NULL) ||
        (desc->confops->pin_config_set == NULL) ||
        ((state->nsettings != 0U) && (state->settings == NULL)))
    {
        return -EINVAL;
    }

    /* Match Linux pinctrl ordering: apply all mux settings first. */
    for (index = 0U; index < state->nsettings; index++)
    {
        setting = &state->settings[index];
        if (setting->pin >= desc->npins)
        {
            return -EINVAL;
        }

        result = desc->pmxops->set_mux(pctldev,
                                       setting->pin,
                                       setting->function);
        if (result != 0)
        {
            return result;
        }
    }

    for (index = 0U; index < state->nsettings; index++)
    {
        setting = &state->settings[index];
        result = desc->confops->pin_config_set(pctldev,
                                               setting->pin,
                                               setting->config);
        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

int pinctrl_select_state(const struct pinctrl *pinctrl,
                         enum pinctrl_state_id state_id)
{
    size_t index;

    if ((pinctrl == NULL) || (pinctrl->states == NULL))
    {
        return -EINVAL;
    }

    for (index = 0U; index < pinctrl->nstates; index++)
    {
        if (pinctrl->states[index].id == state_id)
        {
            return pinctrl_apply_state(pinctrl->pctldev,
                                       &pinctrl->states[index]);
        }
    }

    return -EINVAL;
}
