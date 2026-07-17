#include "e902_clic.h"

#define E902_CLIC_CONFIG_OFFSET             0x0000UL
#define E902_CLIC_INFO_OFFSET               0x0004UL
#define E902_CLIC_MINTTHRESH_OFFSET         0x0008UL
#define E902_CLIC_INTERRUPT_BASE_OFFSET     0x1000UL
#define E902_CLIC_INTERRUPT_STRIDE          4UL
#define E902_CLIC_INTIP_OFFSET              0UL
#define E902_CLIC_INTIE_OFFSET              1UL
#define E902_CLIC_INTATTR_OFFSET            2UL
#define E902_CLIC_INTCTL_OFFSET             3UL

#define E902_CLIC_INFO_NUM_MASK             0x00001FFFUL
#define E902_CLIC_INFO_CONTROL_BITS_SHIFT   21U
#define E902_CLIC_INFO_CONTROL_BITS_MASK    0xFU
#define E902_CLIC_INFO_ZERO_IRQ_COUNT       256U
#define E902_CLIC_MAX_HARDWARE_IRQS         4096U
#define E902_CLIC_MIN_CONTROL_BITS          2U
#define E902_CLIC_MAX_CONTROL_BITS          5U

#define E902_CLIC_CONFIG_NLBITS_SHIFT       1U
#define E902_CLIC_CONFIG_NLBITS_MASK        0x1EU
#define E902_CLIC_MINTTHRESH_MASK           0xFF000000UL
#define E902_CLIC_INT_ENABLE                0x01U
#define E902_CLIC_ATTR_SHV                  0x01U
#define E902_CLIC_ATTR_TRIGGER_SHIFT        1U
#define E902_MSTATUS_MIE                    0x00000008UL

static struct e902_irq_descriptor *clic_descriptors;
static struct e902_clic_info clic_info;
static uint32_t clic_initialized;

volatile uint32_t g_e902_irq_count;
volatile uint32_t g_e902_last_irq;
volatile uint32_t g_e902_unhandled_irq_count;

static uint8_t mmio_read8(uintptr_t address)
{
    return *(volatile uint8_t *)address;
}

static void mmio_write8(uintptr_t address, uint8_t value)
{
    *(volatile uint8_t *)address = value;
}

static uint32_t mmio_read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

static void mmio_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static uintptr_t clic_get_irq_register_address(
    uint32_t irq,
    uintptr_t offset)
{
    return E902_CLIC_BASE + E902_CLIC_INTERRUPT_BASE_OFFSET +
           ((uintptr_t)irq * E902_CLIC_INTERRUPT_STRIDE) +
           offset;
}

static void clic_set_irq_enabled(uint32_t irq, uint32_t enabled)
{
    uintptr_t address =
        clic_get_irq_register_address(irq, E902_CLIC_INTIE_OFFSET);
    uint8_t value = mmio_read8(address);

    if (enabled != 0U)
    {
        value |= E902_CLIC_INT_ENABLE;
    }
    else
    {
        value &= (uint8_t)~E902_CLIC_INT_ENABLE;
    }
    mmio_write8(address, value);
}

static void clic_fence(void)
{
    __asm__ volatile ("fence iorw, iorw" ::: "memory");
}

static int clic_irq_is_valid(uint32_t irq)
{
    return (clic_initialized != 0U) &&
           (irq < clic_info.hardware_irq_count) &&
           (irq < clic_info.vector_count);
}

static uint8_t clic_control_mask(void)
{
    if (clic_info.control_bits == 0U)
    {
        return 0U;
    }

    return (uint8_t)(0xFFU << (8U - clic_info.control_bits));
}

uint32_t e902_global_irq_disable(void)
{
    uint32_t previous_mstatus;
    uint32_t mask = E902_MSTATUS_MIE;

    __asm__ volatile ("csrrc %0, mstatus, %1"
                      : "=r"(previous_mstatus)
                      : "r"(mask)
                      : "memory");
    return previous_mstatus;
}

void e902_global_irq_enable(void)
{
    __asm__ volatile ("csrsi mstatus, 8" ::: "memory");
}

void e902_global_irq_restore(uint32_t previous_mstatus)
{
    if ((previous_mstatus & E902_MSTATUS_MIE) != 0U)
    {
        e902_global_irq_enable();
    }
    else
    {
        (void)e902_global_irq_disable();
    }
}

int e902_clic_init(const uintptr_t *vector_table,
                   uint32_t vector_count,
                   struct e902_irq_descriptor *descriptors,
                   uint32_t descriptor_count)
{
    uint32_t raw_info;
    uint32_t hardware_irq_count;
    uint32_t control_bits;
    uint32_t mtvt_readback;
    uint32_t irq;
    uint32_t usable_irq_count;
    uint8_t cliccfg;

    if ((vector_table == 0) || (descriptors == 0) ||
        (vector_count == 0U) || (descriptor_count < vector_count) ||
        ((((uintptr_t)vector_table) &
          (E902_CLIC_VECTOR_ALIGNMENT - 1U)) != 0U))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    (void)e902_global_irq_disable();
    clic_initialized = 0U;
    clic_descriptors = 0;

    raw_info = mmio_read32(E902_CLIC_BASE + E902_CLIC_INFO_OFFSET);
    hardware_irq_count = raw_info & E902_CLIC_INFO_NUM_MASK;
    if (hardware_irq_count == 0U)
    {
        hardware_irq_count = E902_CLIC_INFO_ZERO_IRQ_COUNT;
    }

    control_bits =
        (raw_info >> E902_CLIC_INFO_CONTROL_BITS_SHIFT) &
        E902_CLIC_INFO_CONTROL_BITS_MASK;
    if ((hardware_irq_count > E902_CLIC_MAX_HARDWARE_IRQS) ||
        (control_bits < E902_CLIC_MIN_CONTROL_BITS) ||
        (control_bits > E902_CLIC_MAX_CONTROL_BITS))
    {
        return E902_CLIC_ERROR_INFO;
    }

    for (irq = 0U; irq < hardware_irq_count; irq++)
    {
        clic_set_irq_enabled(irq, 0U);
    }

    __asm__ volatile ("csrw mtvt, %0"
                      :
                      : "r"((uint32_t)(uintptr_t)vector_table)
                      : "memory");
    __asm__ volatile ("csrr %0, mtvt" : "=r"(mtvt_readback));
    if (mtvt_readback != (uint32_t)(uintptr_t)vector_table)
    {
        return E902_CLIC_ERROR_VECTOR;
    }

    cliccfg = (uint8_t)((control_bits << E902_CLIC_CONFIG_NLBITS_SHIFT) &
                        E902_CLIC_CONFIG_NLBITS_MASK);
    mmio_write8(E902_CLIC_BASE + E902_CLIC_CONFIG_OFFSET, cliccfg);
    mmio_write32(E902_CLIC_BASE + E902_CLIC_MINTTHRESH_OFFSET, 0U);
    clic_fence();
    if (((mmio_read8(E902_CLIC_BASE + E902_CLIC_CONFIG_OFFSET) &
          E902_CLIC_CONFIG_NLBITS_MASK) != cliccfg) ||
        ((mmio_read32(E902_CLIC_BASE + E902_CLIC_MINTTHRESH_OFFSET) &
          E902_CLIC_MINTTHRESH_MASK) != 0U))
    {
        return E902_CLIC_ERROR_CONFIG;
    }

    clic_descriptors = descriptors;
    clic_info.raw = raw_info;
    clic_info.hardware_irq_count = hardware_irq_count;
    clic_info.vector_count = vector_count;
    clic_info.control_bits = (uint8_t)control_bits;

    for (irq = 0U; irq < vector_count; irq++)
    {
        descriptors[irq].handler = 0;
        descriptors[irq].parameter = 0;
    }

    usable_irq_count = hardware_irq_count;
    if (usable_irq_count > vector_count)
    {
        usable_irq_count = vector_count;
    }

    for (irq = 0U; irq < usable_irq_count; irq++)
    {
        mmio_write8(
            clic_get_irq_register_address(irq, E902_CLIC_INTATTR_OFFSET),
            E902_CLIC_ATTR_SHV);
        mmio_write8(
            clic_get_irq_register_address(irq, E902_CLIC_INTCTL_OFFSET),
            0U);
    }

    g_e902_irq_count = 0U;
    g_e902_last_irq = 0U;
    g_e902_unhandled_irq_count = 0U;
    clic_initialized = 1U;
    clic_fence();

    return E902_CLIC_OK;
}

int e902_clic_register_irq(uint32_t irq,
                           e902_irq_handler_t handler,
                           void *parameter,
                           enum e902_clic_trigger trigger,
                           uint8_t control)
{
    uint32_t previous_mstatus;
    uint8_t attribute;

    if (!clic_irq_is_valid(irq) || (handler == 0) ||
        ((trigger != E902_CLIC_TRIGGER_HIGH_LEVEL) &&
         (trigger != E902_CLIC_TRIGGER_POSITIVE_EDGE) &&
         (trigger != E902_CLIC_TRIGGER_NEGATIVE_EDGE)))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    previous_mstatus = e902_global_irq_disable();
    clic_set_irq_enabled(irq, 0U);
    clic_fence();
    if (trigger != E902_CLIC_TRIGGER_HIGH_LEVEL)
    {
        mmio_write8(
            clic_get_irq_register_address(irq, E902_CLIC_INTIP_OFFSET),
            0U);
    }

    clic_descriptors[irq].parameter = parameter;
    clic_descriptors[irq].handler = handler;

    attribute = E902_CLIC_ATTR_SHV |
                (uint8_t)((uint8_t)trigger <<
                          E902_CLIC_ATTR_TRIGGER_SHIFT);
    mmio_write8(
        clic_get_irq_register_address(irq, E902_CLIC_INTATTR_OFFSET),
        attribute);
    mmio_write8(
        clic_get_irq_register_address(irq, E902_CLIC_INTCTL_OFFSET),
        control & clic_control_mask());
    clic_fence();
    e902_global_irq_restore(previous_mstatus);

    return E902_CLIC_OK;
}

int e902_clic_enable_irq(uint32_t irq)
{
    if (!clic_irq_is_valid(irq) ||
        (clic_descriptors[irq].handler == 0))
    {
        return E902_CLIC_ERROR_STATE;
    }

    clic_fence();
    clic_set_irq_enabled(irq, 1U);
    clic_fence();
    return E902_CLIC_OK;
}

int e902_clic_disable_irq(uint32_t irq)
{
    if (!clic_irq_is_valid(irq))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    clic_set_irq_enabled(irq, 0U);
    clic_fence();
    return E902_CLIC_OK;
}

int e902_clic_set_pending(uint32_t irq)
{
    if (!clic_irq_is_valid(irq))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    mmio_write8(
        clic_get_irq_register_address(irq, E902_CLIC_INTIP_OFFSET),
        1U);
    clic_fence();
    return E902_CLIC_OK;
}

int e902_clic_clear_pending(uint32_t irq)
{
    if (!clic_irq_is_valid(irq))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    mmio_write8(
        clic_get_irq_register_address(irq, E902_CLIC_INTIP_OFFSET),
        0U);
    clic_fence();
    return E902_CLIC_OK;
}

int e902_clic_get_pending(uint32_t irq, uint32_t *pending)
{
    if (!clic_irq_is_valid(irq) || (pending == 0))
    {
        return E902_CLIC_ERROR_ARGUMENT;
    }

    *pending = mmio_read8(
        clic_get_irq_register_address(
            irq, E902_CLIC_INTIP_OFFSET)) & 1U;
    return E902_CLIC_OK;
}

const struct e902_clic_info *e902_clic_get_info(void)
{
    if (clic_initialized == 0U)
    {
        return 0;
    }

    return &clic_info;
}

void e902_irq_dispatch(struct e902_exception_frame *frame)
{
    uint32_t irq;

    if ((frame == 0) ||
        ((frame->mcause & E902_MCAUSE_INTERRUPT_MASK) == 0U))
    {
        g_e902_unhandled_irq_count++;
        return;
    }

    irq = frame->mcause & E902_MCAUSE_CODE_MASK;
    g_e902_irq_count++;
    g_e902_last_irq = irq;

    if (clic_irq_is_valid(irq) &&
        (clic_descriptors[irq].handler != 0))
    {
        clic_descriptors[irq].handler(
            irq, clic_descriptors[irq].parameter, frame);
        return;
    }

    if ((clic_initialized != 0U) &&
        (irq < clic_info.hardware_irq_count))
    {
        clic_set_irq_enabled(irq, 0U);
        clic_fence();
    }
    g_e902_unhandled_irq_count++;
}
