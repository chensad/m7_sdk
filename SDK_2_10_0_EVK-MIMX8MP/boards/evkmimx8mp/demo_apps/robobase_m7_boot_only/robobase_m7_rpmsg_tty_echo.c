/*
 * Minimal i.MX8MP CM7 RPMsg tty echo firmware for RoboBase.
 *
 * This intentionally avoids BOARD_InitHardware(), debug UART, full-board pinmux, RDC and
 * clock-tree setup. Linux remoteproc owns the boot flow; this firmware only
 * initializes the safety GPIOs and MU-backed RPMsg-Lite transport. A minimal
 * MPU region gives the SoC peripheral window Device memory attributes before
 * any peripheral access; it does not enable caches or run full board setup.
 */

#include <stdint.h>
#include <string.h>

#include "fsl_device_registers.h"
#include "mpu_armv7.h"
#include "robobase_safe_app.h"
#include "rpmsg_lite.h"
#include "rpmsg_ns.h"
#include "rpmsg_platform.h"
#include "system_MIMX8ML8_cm7.h"

#define ROBOBASE_RPMSG_SHMEM_BASE ((void *)0x55000000U)
#define ROBOBASE_RPMSG_LOCAL_EPT_ADDR (30U)
#define ROBOBASE_RPMSG_CHANNEL_NAME "rpmsg-virtual-tty-channel-1"
#define ROBOBASE_RPMSG_MAX_PAYLOAD (496U)
#define ROBOBASE_NS_DELAY_LOOPS (8000000U)

static struct rpmsg_lite_instance s_rpmsg_context;
static struct rpmsg_lite_ept_static_context s_ept_context;

static volatile uint32_t s_rx_pending;
static volatile uint32_t s_rx_src;
static volatile uint32_t s_rx_len;
static uint8_t s_rx_buf[ROBOBASE_RPMSG_MAX_PAYLOAD];
static uint8_t s_work_rx_buf[ROBOBASE_RPMSG_MAX_PAYLOAD];
static uint8_t s_tx_buf[ROBOBASE_RPMSG_MAX_PAYLOAD];

void SysTick_Handler(void)
{
    robobase_safe_tick_1ms();
}

static void robobase_idle_forever(void)
{
    for (;;)
    {
        __WFI();
    }
}

/*
 * The Cortex-M7 default map treats 0x30000000 as Normal memory. MMIO must
 * instead be Device memory so peripheral transactions cannot be merged like
 * RAM stores. Without this, GPIO/IOMUX writes were observed to disturb adjacent
 * registers on MYD-JX8MP, losing safety_allow's mux and output direction.
 *
 * Reserve the highest implemented MPU region for 0x30000000..0x30ffffff.
 * This standalone firmware has no other MPU region owner. Revisit that
 * reservation if another MPU setup is introduced. TCM, code and RPMsg shared
 * memory retain their existing attributes; cache state is unchanged.
 */
static int robobase_init_mmio_memory(void)
{
    uint32_t saved_primask = __get_PRIMASK();
    uint32_t region_count = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
    uint32_t saved_ctrl = MPU->CTRL;

    if (region_count == 0U)
    {
        return 0;
    }

    __disable_irq();
    __DSB();
    ARM_MPU_Disable();
    ARM_MPU_SetRegionEx(region_count - 1U, 0x30000000U,
                       ARM_MPU_RASR(1U, ARM_MPU_AP_FULL, 0U, 1U, 0U, 1U, 0U, ARM_MPU_REGION_SIZE_16MB));
    ARM_MPU_Enable(saved_ctrl | MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);
    __set_PRIMASK(saved_primask);
    return 1;
}

static void robobase_delay_for_linux_ns(void)
{
    volatile uint32_t loops = ROBOBASE_NS_DELAY_LOOPS;

    while (loops-- != 0U)
    {
        __NOP();
    }
}

static void robobase_safe_init_timer(void)
{
    SystemCoreClockUpdate();
    if (SystemCoreClock == 0U)
    {
        SystemCoreClock = DEFAULT_SYSTEM_CLOCK;
    }

    if (SysTick_Config(SystemCoreClock / 1000U) != 0U)
    {
        robobase_idle_forever();
    }
}

static int32_t robobase_rpmsg_rx_cb(void *payload, uint32_t payload_len, uint32_t src, void *priv)
{
    uint32_t copy_len = payload_len;

    (void)priv;

    if (payload == NULL)
    {
        return RL_RELEASE;
    }

    if (copy_len > ROBOBASE_RPMSG_MAX_PAYLOAD)
    {
        copy_len = ROBOBASE_RPMSG_MAX_PAYLOAD;
    }

    if ((copy_len != 0U) && (s_rx_pending == 0U))
    {
        memcpy(s_rx_buf, payload, copy_len);
        s_rx_src = src;
        s_rx_len = copy_len;
        __DMB();
        s_rx_pending = 1U;
    }

    return RL_RELEASE;
}

int main(void)
{
    struct rpmsg_lite_instance *rpmsg;
    struct rpmsg_lite_endpoint *ept;

    if (robobase_init_mmio_memory() == 0)
    {
        robobase_idle_forever();
    }
    robobase_safe_init();
    robobase_safe_init_timer();

    rpmsg = rpmsg_lite_remote_init(ROBOBASE_RPMSG_SHMEM_BASE, RL_PLATFORM_IMX8MP_M7_USER_LINK_ID, RL_NO_FLAGS,
                                   &s_rpmsg_context);
    if (rpmsg == NULL)
    {
        robobase_idle_forever();
    }

    if (rpmsg_lite_wait_for_link_up(rpmsg, RL_BLOCK) == 0U)
    {
        robobase_idle_forever();
    }

    ept = rpmsg_lite_create_ept(rpmsg, ROBOBASE_RPMSG_LOCAL_EPT_ADDR, robobase_rpmsg_rx_cb, NULL, &s_ept_context);
    if (ept == NULL)
    {
        robobase_idle_forever();
    }

    robobase_delay_for_linux_ns();
    if (rpmsg_ns_announce(rpmsg, ept, ROBOBASE_RPMSG_CHANNEL_NAME, RL_NS_CREATE) != RL_SUCCESS)
    {
        robobase_idle_forever();
    }

    for (;;)
    {
        uint8_t did_work = 0U;
        uint32_t rx_len;
        uint32_t tx_len;
        uint32_t dst;

        while (robobase_safe_process_pending_tick() != 0U)
        {
            did_work = 1U;
        }

        __disable_irq();
        if (s_rx_pending == 0U)
        {
            if (did_work != 0U)
            {
                __enable_irq();
                continue;
            }
            __DSB();
            __WFI();
            __enable_irq();
            continue;
        }

        rx_len = s_rx_len;
        dst = s_rx_src;
        memcpy(s_work_rx_buf, s_rx_buf, rx_len);
        s_rx_pending = 0U;
        __DMB();
        __enable_irq();

        tx_len = robobase_safe_handle_frame(s_work_rx_buf, rx_len, s_tx_buf, ROBOBASE_RPMSG_MAX_PAYLOAD);
        if (tx_len == 0U)
        {
            tx_len = rx_len;
            memcpy(s_tx_buf, s_work_rx_buf, tx_len);
        }

        (void)rpmsg_lite_send(rpmsg, ept, dst, (char *)s_tx_buf, tx_len, RL_BLOCK);
    }
}
