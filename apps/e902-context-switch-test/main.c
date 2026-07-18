#include <stddef.h>
#include <stdint.h>

#include "board.h"
#include "context_self_test.h"
#include "e902_clic.h"
#include "e902_context.h"
#include "t22_serdes_irq.h"

#define CONTEXT_TEST_STACK_SIZE              1024U
#define CONTEXT_TEST_STACK_GUARD_SIZE          64U
#define CONTEXT_TEST_STACK_PATTERN           0xA5U
#define CONTEXT_TEST_LOOP_COUNT                 8U
#define CONTEXT_TEST_WAIT_LIMIT           1000000U
#define CONTEXT_TEST_PARAMETER_A          0xC07EA001U
#define CONTEXT_TEST_PARAMETER_B          0xC07EB002U

#define CONTEXT_TEST_OK                         0U
#define CONTEXT_TEST_FAIL_IRQ_INIT              1U
#define CONTEXT_TEST_FAIL_CONTEXT_INIT          2U
#define CONTEXT_TEST_FAIL_STACK_INIT            3U
#define CONTEXT_TEST_FAIL_INITIAL_FRAME         4U
#define CONTEXT_TEST_FAIL_FIRST_THREAD          5U
#define CONTEXT_TEST_FAIL_MERGED_REQUEST        6U
#define CONTEXT_TEST_FAIL_A_REGISTERS           7U
#define CONTEXT_TEST_FAIL_B_REGISTERS           8U
#define CONTEXT_TEST_FAIL_COUNTS                9U
#define CONTEXT_TEST_FAIL_STACK_GUARD          10U
#define CONTEXT_TEST_FAIL_FINAL_STATE          11U
#define CONTEXT_TEST_FAIL_THREAD_EXIT           12U
#define CONTEXT_TEST_FAIL_UNEXPECTED_RETURN     13U

#define CONTEXT_TEST_EXPECTED_REQUESTS  \
    ((2U * CONTEXT_TEST_LOOP_COUNT) + 4U)
#define CONTEXT_TEST_EXPECTED_SWITCHES  \
    ((2U * CONTEXT_TEST_LOOP_COUNT) + 3U)

struct context_test_parameter
{
    uint32_t value;
};

static uint8_t thread_a_stack[CONTEXT_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static uint8_t thread_b_stack[CONTEXT_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static volatile e902_ubase_t thread_a_sp;
static volatile e902_ubase_t thread_b_sp;
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

volatile uint32_t g_context_test_result;
volatile int32_t g_context_test_init_status;
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

static int stack_pointer_is_valid(e902_ubase_t sp, const uint8_t *stack)
{
    e902_ubase_t lower = (e902_ubase_t)stack +
                         CONTEXT_TEST_STACK_GUARD_SIZE;
    e902_ubase_t upper = (e902_ubase_t)stack +
                         CONTEXT_TEST_STACK_SIZE;

    return (sp >= lower) && (sp < upper) &&
           ((sp & (E902_CONTEXT_STACK_ALIGNMENT - 1UL)) == 0UL);
}

static int register_snapshot_is_valid(
    const struct context_test_registers *registers,
    const uint8_t *stack)
{
    return (registers->ra == CONTEXT_TEST_MARKER_RA) &&
           stack_pointer_is_valid(registers->sp, stack) &&
           (registers->gp ==
            (uint32_t)(e902_ubase_t)&__global_pointer$) &&
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
    const e902_context_frame_t *frame,
    void *entry,
    void *parameter,
    void *exit,
    const uint8_t *stack)
{
    e902_ubase_t expected_top =
        ((e902_ubase_t)stack + CONTEXT_TEST_STACK_SIZE) &
        ~((e902_ubase_t)E902_CONTEXT_STACK_ALIGNMENT - 1UL);

    return ((e902_ubase_t)frame ==
            (expected_top - sizeof(e902_context_frame_t))) &&
           (frame->ra == (uint32_t)(e902_ubase_t)exit) &&
           (frame->sp == (uint32_t)expected_top) &&
           (frame->gp ==
            (uint32_t)(e902_ubase_t)&__global_pointer$) &&
           (frame->tp == 0U) &&
           (frame->a0 == (uint32_t)(e902_ubase_t)parameter) &&
           (frame->mepc == (uint32_t)(e902_ubase_t)entry) &&
           (frame->mstatus == E902_CONTEXT_INITIAL_MSTATUS) &&
           (frame->mcause == 0U) &&
           (frame->mtval == 0U) &&
           (frame->reserved == 0U);
}

static int run_merged_request_test(void)
{
    e902_base_t level;
    uint32_t timeout;

    level = rt_hw_interrupt_disable();
    rt_hw_context_switch((e902_ubase_t)&thread_a_sp,
                         (e902_ubase_t)&thread_b_sp);
    rt_hw_context_switch_interrupt((e902_ubase_t)&thread_b_sp,
                                   (e902_ubase_t)&thread_a_sp);
    rt_hw_interrupt_enable(level);

    timeout = CONTEXT_TEST_WAIT_LIMIT;
    while ((g_e902_context_switch_count < 1U) && (timeout != 0U))
    {
        timeout--;
        __asm__ volatile ("nop");
    }

    return (timeout != 0U) &&
           (g_context_test_thread_a_entries == 1U) &&
           (g_context_test_thread_b_entries == 0U) &&
           (g_e902_context_request_count == 2U) &&
           (g_e902_context_irq_count == 1U) &&
           (g_e902_context_switch_count == 1U);
}

static void context_test_finish(uint32_t result)
{
    uint32_t pending = 0xFFFFFFFFU;

    (void)rt_hw_interrupt_disable();
    g_context_test_result = result;
    (void)e902_clic_get_pending(E902_CONTEXT_SWITCH_IRQ, &pending);

    (void)board_early_puts("E902 context self-test: requests=");
    put_hex32(g_e902_context_request_count);
    (void)board_early_puts(" irqs=");
    put_hex32(g_e902_context_irq_count);
    (void)board_early_puts(" switches=");
    put_hex32(g_e902_context_switch_count);
    (void)board_early_puts("\n");

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
        (void)board_early_puts(" context_error=");
        put_hex32((uint32_t)g_e902_context_last_error);
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
        (e902_ubase_t)&thread_b_sp,
        (e902_ubase_t)&thread_a_sp,
        &thread_b_registers);
    if (!register_snapshot_is_valid(&thread_b_registers, thread_b_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_B_REGISTERS);
    }
    g_context_test_thread_b_registers_valid = 1U;

    for (index = 0U; index < CONTEXT_TEST_LOOP_COUNT; index++)
    {
        g_context_test_thread_b_requests++;
        rt_hw_context_switch_interrupt((e902_ubase_t)&thread_b_sp,
                                       (e902_ubase_t)&thread_a_sp);
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
        (e902_ubase_t)&thread_a_sp,
        (e902_ubase_t)&thread_b_sp,
        &thread_a_registers);
    if (!register_snapshot_is_valid(&thread_a_registers, thread_a_stack))
    {
        context_test_finish(CONTEXT_TEST_FAIL_A_REGISTERS);
    }
    g_context_test_thread_a_registers_valid = 1U;

    for (index = 0U; index < CONTEXT_TEST_LOOP_COUNT; index++)
    {
        g_context_test_thread_a_requests++;
        rt_hw_context_switch((e902_ubase_t)&thread_a_sp,
                             (e902_ubase_t)&thread_b_sp);
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
         (CONTEXT_TEST_LOOP_COUNT - 1U)) ||
        (g_e902_context_request_count !=
         CONTEXT_TEST_EXPECTED_REQUESTS) ||
        (g_e902_context_irq_count != CONTEXT_TEST_EXPECTED_SWITCHES) ||
        (g_e902_context_switch_count != CONTEXT_TEST_EXPECTED_SWITCHES))
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

    pending = 0xFFFFFFFFU;
    if ((e902_clic_get_pending(E902_CONTEXT_SWITCH_IRQ, &pending) !=
         E902_CLIC_OK) ||
        (pending != 0U) ||
        (rt_thread_switch_interrupt_flag != 0UL) ||
        (g_e902_context_last_error != 0) ||
        (g_e902_context_halted != 0U) ||
        (g_context_test_thread_exit_called != 0U) ||
        (g_e902_irq_count != CONTEXT_TEST_EXPECTED_SWITCHES) ||
        (g_e902_last_irq != E902_CONTEXT_SWITCH_IRQ) ||
        (g_e902_unhandled_irq_count != 0U))
    {
        context_test_finish(CONTEXT_TEST_FAIL_FINAL_STATE);
    }

    context_test_finish(CONTEXT_TEST_OK);
}

int main(void)
{
    e902_context_frame_t *frame_a;
    e902_context_frame_t *frame_b;
    uint8_t *stack_pointer;
    int result;

    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)board_early_puts("E902 context self-test: init\n");

    result = t22_serdes_irq_init();
    g_context_test_init_status = result;
    if (result != E902_CLIC_OK)
    {
        context_test_finish(CONTEXT_TEST_FAIL_IRQ_INIT);
    }

    result = e902_context_switch_init();
    g_context_test_init_status = result;
    if (result != E902_CONTEXT_OK)
    {
        context_test_finish(CONTEXT_TEST_FAIL_CONTEXT_INIT);
    }

    fill_stack(thread_a_stack);
    fill_stack(thread_b_stack);

    stack_pointer = rt_hw_stack_init(
        (void *)context_test_thread_a,
        &thread_a_parameter,
        &thread_a_stack[CONTEXT_TEST_STACK_SIZE - sizeof(e902_ubase_t)],
        (void *)context_test_thread_exit);
    thread_a_sp = (e902_ubase_t)stack_pointer;
    stack_pointer = rt_hw_stack_init(
        (void *)context_test_thread_b,
        &thread_b_parameter,
        &thread_b_stack[CONTEXT_TEST_STACK_SIZE - sizeof(e902_ubase_t)],
        (void *)context_test_thread_exit);
    thread_b_sp = (e902_ubase_t)stack_pointer;

    if ((thread_a_sp == 0UL) || (thread_b_sp == 0UL))
    {
        context_test_finish(CONTEXT_TEST_FAIL_STACK_INIT);
    }

    frame_a = (e902_context_frame_t *)thread_a_sp;
    frame_b = (e902_context_frame_t *)thread_b_sp;
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
    rt_hw_context_switch_to((e902_ubase_t)&thread_a_sp);
    context_test_finish(CONTEXT_TEST_FAIL_UNEXPECTED_RETURN);
}
