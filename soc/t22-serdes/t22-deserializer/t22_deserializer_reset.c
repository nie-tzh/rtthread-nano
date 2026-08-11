#include "t22_deserializer_reset.h"
#include "t22_reset.h"
#include "t22_serdes.h"
#include "t22_serdes_reset.h"

#define T22_DESERIALIZER_CSR_SOFT_RESET_OFFSET    0x024U

const struct t22_reset_soc_data t22_deserializer_reset_data =
{
    .reset_reg = T22_SERDES_CSR_BASE +
                 T22_DESERIALIZER_CSR_SOFT_RESET_OFFSET,
    .nr_resets = T22_SERDES_RESET_COUNT
};
