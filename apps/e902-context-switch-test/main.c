#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "context_self_test.h"
#include "e902.h"
#include "riscv_clic.h"
#include "t22_serdes_irq.h"

#define CONTEXT_TEST_STACK_SIZE              1024U
#define CONTEXT_TEST_STACK_GUARD_SIZE          64U
#define CONTEXT_TEST_STACK_PATTERN           0xA5U
#define CONTEXT_TEST_LOOP_COUNT                 8U
#define CONTEXT_TEST_WAIT_LIMIT           1000000U
#define CONTEXT_TEST_SWITCH_IRQ \
    T22_SERDES_IRQ_MACHINE_SOFTWARE
#define CONTEXT_TEST_INITIAL_MSTATUS \
    (E902_MSTATUS_MPP_MACHINE | E902_MSTATUS_MPIE_MASK)
#define CONTEXT_TEST_PARAMETER_A          0xC07EA001U
#define CONTEXT_TEST_PARAMETER_B          0xC07EB002U

#define CONTEXT_TEST_OK                         0U
#define CONTEXT_TEST_FAIL_STACK_INIT            1U
#define CONTEXT_TEST_FAIL_INITIAL_FRAME         2U
#define CONTEXT_TEST_FAIL_FIRST_THREAD          3U
#define CONTEXT_TEST_FAIL_MERGED_REQUEST        4U
#define CONTEXT_TEST_FAIL_A_REGISTERS           5U
#define CONTEXT_TEST_FAIL_B_REGISTERS           6U
#define CONTEXT_TEST_FAIL_COUNTS                7U
#define CONTEXT_TEST_FAIL_STACK_GUARD           8U
#define CONTEXT_TEST_FAIL_FINAL_STATE           9U
#define CONTEXT_TEST_FAIL_THREAD_EXIT           10U
#define CONTEXT_TEST_FAIL_UNEXPECTED_RETURN     11U

struct context_test_parameter
{
    uint32_t value;
};

static uint8_t thread_a_stack[CONTEXT_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static uint8_t thread_b_stack[CONTEXT_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static volatile rt_ubase_t thread_a_sp;
static volatile rt_ubase_t thread_b_sp;
static struct context_test_registers thread_a_registers;
static struct context_test_registers thread_b_registers;
static struct context_test_parameter thread_a_parameter =
{
    .value = CONTEXT_TEST_PARAMETER_A
};
static struct context_test_parameter thread_b_parameter =
{
    .value = CONTEXT_TEST_PARAMETER_B
};
static struct riscv_clic *context_test_clic;

volatile uint32_t g_context_test_result;
volatile uint32_t g_context_test_thread_a_entries;
volatile uint32_t g_context_test_thread_b_entries;
volatile uint32_t g_context_test_thread_a_parameter_valid;
volatile uint32_t g_context_test_thread_b_parameter_valid;
volatile uint32_t g_context_test_thread_a_registers_valid;
volatile uint32_t g_context_test_thread_b_registers_valid;
volatile uint32_t g_context_test_thread_a_requests;
volatile uint32_t g_context_test_thread_a_returns;
volatile uint32_t g_context_test_thread_b_requests;
volatile uint32_t g_context_test_thread_b_returns;
volatile uint32_t g_context_test_thread_exit_called;

extern char __global_pointer$;

static void context_test_thread_a(void *parameter);
static void context_test_thread_b(void *parameter);
static void context_test_thread_exit(void);
static void context_test_finish(uint32_t result) __attribute__((noreturn));

static void put_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    (void)board_early_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        (void)board_early_putc(
            digits[(value >> (uint32_t)shift) & 0xFU]);
    }
}

static void fill_stack(uint8_t *stack)
{
    uint32_t index;

    for (index = 0U; index < CONTEXT_TEST_STACK_SIZE; index++)
    {
        stack[index] = CONTEXT_TEST_STACK_PATTERN;
    }
}

static int stack_guard_is_valid(const uint8_t *stack)
{
    uint32_t index;

    for (index = 0U; index < CONTEXT_TEST_STACK_GUARD_SIZE; index++)
    {
        if (stack[index] != CONTEXT_TEST_STACK_PATTERN)
        {
            return 0;
        }
    }

    return 1;
}

static int stack_pointer_is_valid(rt_ubase_t sp, const uint8_t *stack)
{
    rt_ubase_t lower = (rt_ubase_t)stack +
                         CONTEXT_TEST_STACK_GUARD_SIZE;
    rt_ubase_t upper = (rt_ubase_t)stack +
                         CONTEXT_TEST_STACK_SIZE;

    return (sp >= lower) && (sp < upper) &&
           ((sp & (RT_ALIGN_SIZE - 1UL)) == 0UL);
}

static int register_snapshot_is_valid(
    const struct context_test_registers *registers,
    const uint8_t *stack)
{
    return (registers->ra == CONTEXT_TEST_MARKER_RA) &&
           stack_pointer_is_valid(registers->sp, stack) &&
           (registers->gp ==
            (uint32_t)(rt_ubase_t)&__global_pointer$) &&
           (registers->tp == CONTEXT_TEST_MARKER_TP) &&
           (registers->t0 == CONTEXT_TEST_MARKER_T0) &&
           (registers->t1 == CONTEXT_TEST_MARKER_T1) &&
           (registers->t2 == CONTEXT_TEST_MARKER_T2) &&
           (registers->s0 == CONTEXT_TEST_MARKER_S0) &&
           (registers->s1 == CONTEXT_TEST_MARKER_S1) &&
           (registers->a0 == CONTEXT_TEST_MARKER_A0) &&
           (registers->a1 == CONTEXT_TEST_MARKER_A1) &&
           (registers->a2 == CONTEXT_TEST_MARKER_A2) &&
           (registers->a3 == CONTEXT_TEST_MARKER_A3) &&
           (registers->a4 == CONTEXT_TEST_MARKER_A4) &&
           (registers->a5 == CONTEXT_TEST_MARKER_A5);
}

static int initial_frame_is_valid(
    const struct e902_frame *frame,
    void *entry,
    void *parameter,
    void *exit,
    const uint8_t *stack)
{
    rt_ubase_t expected_top =
        ((rt_ubase_t)stack + CONTEXT_TEST_STACK_SIZE) &
        ~((rt_ubase_t)RT_ALIGN_SIZE - 1UL);

    return ((rt_ubase_t)frame ==
            (expected_top - sizeof(struct e902_frame))) &&
           (frame->ra == (uint32_t)(rt_ubase_t)exit) &&
           (frame->sp == (uint32_t)expected_top) &&
           (frame->gp ==
            (uint32_t)(rt_ubase_t)&__global_pointer$) &&
           (frame->tp == 0U) &&
           (frame->a0 == (uint32_t)(rt_ubase_t)parameter) &&
           (frame->mepc == (uint32_t)(rt_ubase_t)entry) &&
           (frame->mstatus == CONTEXT_TEST_INITIAL_MSTATUS) &&
           (frame->mcause == 0U) &&
           (frame->mtval == 0U) &&
           (frame->reserved == 0U);
}

static int run_merged_request_test(void)
{
    rt_base_t level;
    uint32_t pending;
    uint32_t timeout;

    level = rt_hw_interrupt_disable();
    rt_hw_context_switch((rt_ubase_t)&thread_a_sp,
                         (rt_ubase_t)&thread_b_sp);
    rt_hw_context_switch_interrupt((rt_ubase_t)&thread_b_sp,
                                   (rt_ubase_t)&thread_a_sp);
    rt_hw_interrupt_enable(level);

    timeout = CONTEXT_TEST_WAIT_LIMIT;
    pending = 1U;
    while ((pending != 0U) && (timeout != 0U))
    {
        pending = riscv_clic_get_pending(context_test_clic,
                                         CONTEXT_TEST_SWITCH_IRQ);
        timeout--;
        __asm__ volatile ("nop");
    }

    return (timeout != 0U) &&
           (g_context_test_thread_a_entries == 1U) &&
           (g_context_test_thread_b_entries == 0U);
}

static void context_test_finish(uint32_t result)
{
    uint32_t pending = 0xFFFFFFFFU;

    (void)rt_hw_interrupt_disable();
    g_context_test_result = result;
    pending = riscv_clic_get_pending(context_test_clic,
                                     CONTEXT_TEST_SWITCH_IRQ);

    (void)board_early_puts("E902 context self-test: A=");
    put_hex32(g_context_test_thread_a_requests);
    (void)board_early_puts("/");
    put_hex32(g_context_test_thread_a_returns);
    (void)board_early_puts(" B=");
    put_hex32(g_context_test_thread_b_requests);
    (void)board_early_puts("/");
    put_hex32(g_context_test_thread_b_returns);
    (void)board_early_puts(" pending=");
    put_hex32(pending);
    (void)board_early_puts("\n");

    if (result == CONTEXT_TEST_OK)
    {
        (void)board_early_puts("E902 context self-test: PASS\n");
    }
    else
    {
        (void)board_early_puts("E902 context self-test: FAIL result=");
        put_hex32(result);
        (void)board_early_puts("\n");
    }

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static void context_test_thread_exit(void)
{
    g_context_test_thread_exit_called = 1U;
    context_test_finish(CONTEXT_TEST_FAIL_THREAD_EXIT);
}

static void context_test_thread_b(void *parameter)
{
    uint32_t index;

    g_context_test_thread_b_entries++;
    g_context_test_thread_b_parameter_valid =
        ((parameter == &thread_b_parameter) &&
         (((const struct context_test_parameter *)parameter)->value ==
          CONTEXT_TEST_PARAMETER_B)) ? 1U : 0U;

    e902_context_register_self_test(
        (rt_ubase_t)&thread_b_sp,
        (rt_ubase_t)&thread_a_sp,
        &thread_b_registers);
    if (!register_snapshot_is_valid(&thread_b_registers, thread_b_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_B_REGISTERS);
    }
    g_context_test_thread_b_registers_valid = 1U;

    for (index = 0U; index < CONTEXT_TEST_LOOP_COUNT; index++)
    {
        g_context_test_thread_b_requests++;
        rt_hw_context_switch_interrupt((rt_ubase_t)&thread_b_sp,
                                       (rt_ubase_t)&thread_a_sp);
        g_context_test_thread_b_returns++;
    }

    context_test_finish(CONTEXT_TEST_FAIL_UNEXPECTED_RETURN);
}

static void context_test_thread_a(void *parameter)
{
    uint32_t index;
    uint32_t pending;

    g_context_test_thread_a_entries++;
    if (g_context_test_thread_a_entries != 1U)
    {
        context_test_finish(CONTEXT_TEST_FAIL_FIRST_THREAD);
    }
    g_context_test_thread_a_parameter_valid =
        ((parameter == &thread_a_parameter) &&
         (((const struct context_test_parameter *)parameter)->value ==
          CONTEXT_TEST_PARAMETER_A)) ? 1U : 0U;

    if (!run_merged_request_test())
    {
        context_test_finish(CONTEXT_TEST_FAIL_MERGED_REQUEST);
    }

    e902_context_register_self_test(
        (rt_ubase_t)&thread_a_sp,
        (rt_ubase_t)&thread_b_sp,
        &thread_a_registers);
    if (!register_snapshot_is_valid(&thread_a_registers, thread_a_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_A_REGISTERS);
    }
    g_context_test_thread_a_registers_valid = 1U;

    for (index = 0U; index < CONTEXT_TEST_LOOP_COUNT; index++)
    {
        g_context_test_thread_a_requests++;
        rt_hw_context_switch((rt_ubase_t)&thread_a_sp,
                             (rt_ubase_t)&thread_b_sp);
        g_context_test_thread_a_returns++;
    }

    if ((g_context_test_thread_a_parameter_valid == 0U) ||
        (g_context_test_thread_b_parameter_valid == 0U) ||
        (g_context_test_thread_a_registers_valid == 0U) ||
        (g_context_test_thread_b_registers_valid == 0U) ||
        (g_context_test_thread_a_entries != 1U) ||
        (g_context_test_thread_b_entries != 1U) ||
        (g_context_test_thread_a_requests != CONTEXT_TEST_LOOP_COUNT) ||
        (g_context_test_thread_a_returns != CONTEXT_TEST_LOOP_COUNT) ||
        (g_context_test_thread_b_requests != CONTEXT_TEST_LOOP_COUNT) ||
        (g_context_test_thread_b_returns !=
         (CONTEXT_TEST_LOOP_COUNT - 1U)))
    {
        context_test_finish(CONTEXT_TEST_FAIL_COUNTS);
    }

    if (!stack_guard_is_valid(thread_a_stack) ||
        !stack_guard_is_valid(thread_b_stack) ||
        !stack_pointer_is_valid(thread_a_sp, thread_a_stack) ||
        !stack_pointer_is_valid(thread_b_sp, thread_b_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_STACK_GUARD);
    }

    pending = riscv_clic_get_pending(context_test_clic,
                                     CONTEXT_TEST_SWITCH_IRQ);
    if ((pending != 0U) ||
        (g_context_test_thread_exit_called != 0U))
    {
        context_test_finish(CONTEXT_TEST_FAIL_FINAL_STATE);
    }

    context_test_finish(CONTEXT_TEST_OK);
}

int main(void)
{
    struct e902_frame *frame_a;
    struct e902_frame *frame_b;
    uint8_t *stack_pointer;
    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)board_early_puts("E902 context self-test: init\n");

    (void)rt_hw_interrupt_disable();
    rt_hw_interrupt_init();
    context_test_clic = t22_serdes_clic();

    fill_stack(thread_a_stack);
    fill_stack(thread_b_stack);

    stack_pointer = rt_hw_stack_init(
        (void *)context_test_thread_a,
        &thread_a_parameter,
        &thread_a_stack[CONTEXT_TEST_STACK_SIZE - sizeof(rt_ubase_t)],
        (void *)context_test_thread_exit);
    thread_a_sp = (rt_ubase_t)stack_pointer;
    stack_pointer = rt_hw_stack_init(
        (void *)context_test_thread_b,
        &thread_b_parameter,
        &thread_b_stack[CONTEXT_TEST_STACK_SIZE - sizeof(rt_ubase_t)],
        (void *)context_test_thread_exit);
    thread_b_sp = (rt_ubase_t)stack_pointer;

    if ((thread_a_sp == 0UL) || (thread_b_sp == 0UL))
    {
        context_test_finish(CONTEXT_TEST_FAIL_STACK_INIT);
    }

    frame_a = (struct e902_frame *)thread_a_sp;
    frame_b = (struct e902_frame *)thread_b_sp;
    if (!initial_frame_is_valid(frame_a,
                                (void *)context_test_thread_a,
                                &thread_a_parameter,
                                (void *)context_test_thread_exit,
                                thread_a_stack) ||
        !initial_frame_is_valid(frame_b,
                                (void *)context_test_thread_b,
                                &thread_b_parameter,
                                (void *)context_test_thread_exit,
                                thread_b_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_INITIAL_FRAME);
    }

    (void)board_early_puts("E902 context self-test: start first thread\n");
    rt_hw_context_switch_to((rt_ubase_t)&thread_a_sp);
    context_test_finish(CONTEXT_TEST_FAIL_UNEXPECTED_RETURN);
}
