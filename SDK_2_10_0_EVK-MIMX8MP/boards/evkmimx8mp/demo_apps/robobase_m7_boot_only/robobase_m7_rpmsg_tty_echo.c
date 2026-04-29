/*
 * Minimal i.MX8MP CM7 RPMsg tty echo firmware for RoboBase.
 *
 * This intentionally avoids BOARD_InitHardware(), debug UART, pinmux, RDC and
 * clock-tree setup. Linux remoteproc owns the boot flow; this firmware only
 * initializes the MU-backed RPMsg-Lite transport and announces one tty channel.
 */

#include <stdint.h>
#include <string.h>

#include "fsl_device_registers.h"
#include "rpmsg_lite.h"
#include "rpmsg_ns.h"
#include "rpmsg_platform.h"

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
static uint8_t s_tx_buf[ROBOBASE_RPMSG_MAX_PAYLOAD];

static void robobase_idle_forever(void)
{
    for (;;)
    {
        __WFI();
    }
}

static void robobase_delay_for_linux_ns(void)
{
    volatile uint32_t loops = ROBOBASE_NS_DELAY_LOOPS;

    while (loops-- != 0U)
    {
        __NOP();
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
        uint32_t len;
        uint32_t dst;

        __disable_irq();
        if (s_rx_pending == 0U)
        {
            __DSB();
            __WFI();
            __enable_irq();
            continue;
        }

        len = s_rx_len;
        dst = s_rx_src;
        memcpy(s_tx_buf, s_rx_buf, len);
        s_rx_pending = 0U;
        __DMB();
        __enable_irq();

        (void)rpmsg_lite_send(rpmsg, ept, dst, (char *)s_tx_buf, len, RL_BLOCK);
    }
}
