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
#include "robobase/rb_safety_proto.h"
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

static uint32_t s_heartbeat_counter;
static uint32_t s_safety_loop_counter;
static uint32_t s_last_linux_seq;
static uint32_t s_last_linux_uptime_ms;
static uint32_t s_last_lease_age_ms;
static uint8_t s_seen_lease;
static uint8_t s_state = RB_SAFE_BOOT;
static uint8_t s_motion_enable;
static uint32_t s_fault_bits;
static uint32_t s_latched_fault_bits;

/*
 * v0.1 uses simulated safe inputs until real GPIO sampling is wired in.
 * Treat NC contacts as closed so software lease behavior can be tested first.
 */
static uint8_t s_estop_nc_closed = 1U;
static uint8_t s_bumper_nc_closed = 1U;

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

static void robobase_safe_update_motion(uint8_t linux_alive, uint8_t upstream_lease_valid, uint8_t driver_ok,
                                        uint8_t power_ok)
{
    s_fault_bits = 0U;

    if (s_estop_nc_closed == 0U)
    {
        s_fault_bits |= RB_FAULT_ESTOP;
        s_latched_fault_bits |= RB_FAULT_ESTOP;
    }

    if (s_bumper_nc_closed == 0U)
    {
        s_fault_bits |= RB_FAULT_BUMPER;
        s_latched_fault_bits |= RB_FAULT_BUMPER;
    }

    if (linux_alive == 0U)
    {
        s_fault_bits |= RB_FAULT_LINUX_TIMEOUT;
    }

    if (upstream_lease_valid == 0U)
    {
        s_fault_bits |= RB_FAULT_UPSTREAM_TIMEOUT;
    }

    if (driver_ok == 0U)
    {
        s_fault_bits |= RB_FAULT_DRIVER_FAULT;
    }

    if (power_ok == 0U)
    {
        s_fault_bits |= RB_FAULT_POWER_FAULT;
    }

    s_motion_enable = (uint8_t)((s_estop_nc_closed != 0U) && (s_bumper_nc_closed != 0U) &&
                                (s_latched_fault_bits == 0U) && (linux_alive != 0U) &&
                                (upstream_lease_valid != 0U) && (driver_ok != 0U) && (power_ok != 0U));

    if (s_latched_fault_bits != 0U)
    {
        s_state = RB_SAFE_FAULT_LATCHED;
    }
    else if (s_motion_enable != 0U)
    {
        s_state = RB_SAFE_RUNNING;
    }
    else if (s_fault_bits != 0U)
    {
        s_state = RB_SAFE_STOP;
    }
    else
    {
        s_state = RB_SAFE_STANDBY;
    }
}

static uint32_t robobase_safe_clear_faults(uint32_t clear_mask)
{
    uint32_t clearable = clear_mask;

    if (s_estop_nc_closed == 0U)
    {
        clearable &= ~RB_FAULT_ESTOP;
    }

    if (s_bumper_nc_closed == 0U)
    {
        clearable &= ~RB_FAULT_BUMPER;
    }

    s_latched_fault_bits &= ~clearable;
    robobase_safe_update_motion(1U, 0U, 1U, 1U);
    return clearable;
}

static uint32_t robobase_safe_build_status(uint32_t seq, uint8_t *buf, uint32_t buf_size)
{
    struct rb_safe_hdr *hdr = (struct rb_safe_hdr *)buf;
    struct rb_safe_status_msg *status = (struct rb_safe_status_msg *)(buf + sizeof(*hdr));
    uint16_t payload_len = (uint16_t)sizeof(*status);

    if (buf_size < (sizeof(*hdr) + sizeof(*status)))
    {
        return 0U;
    }

    rb_safe_hdr_init(hdr, RB_SAFE_MSG_STATUS, seq, payload_len);

    s_heartbeat_counter++;
    status->m7_uptime_ms = s_safety_loop_counter;
    status->heartbeat_counter = s_heartbeat_counter;
    status->state = s_state;
    status->motion_enable = s_motion_enable;
    status->estop_nc_closed = s_estop_nc_closed;
    status->bumper_nc_closed = s_bumper_nc_closed;
    status->fault_bits = s_fault_bits;
    status->latched_fault_bits = s_latched_fault_bits;
    status->last_linux_seq = s_last_linux_seq;
    status->last_lease_age_ms = s_last_lease_age_ms;
    status->safety_loop_counter = s_safety_loop_counter;
    status->reserved = 0U;

    return (uint32_t)(sizeof(*hdr) + sizeof(*status));
}

static uint32_t robobase_safe_handle_frame(const uint8_t *rx, uint32_t rx_len, uint8_t *tx, uint32_t tx_size)
{
    const struct rb_safe_hdr *hdr = (const struct rb_safe_hdr *)rx;
    const uint8_t *payload = rx + sizeof(*hdr);

    if (rx_len < sizeof(*hdr))
    {
        return 0U;
    }

    if (hdr->magic != RB_SAFE_MAGIC)
    {
        return 0U;
    }

    if ((hdr->ver_major != RB_SAFE_VER_MAJOR) || (hdr->hdr_len != sizeof(*hdr)) ||
        (hdr->crc32 != RB_SAFE_CRC_DISABLED) || (rx_len != (sizeof(*hdr) + hdr->payload_len)))
    {
        s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
        return robobase_safe_build_status(hdr->seq, tx, tx_size);
    }

    if (hdr->msg_type == RB_SAFE_MSG_LEASE)
    {
        const struct rb_safe_lease_msg *lease = (const struct rb_safe_lease_msg *)payload;

        if (hdr->payload_len != sizeof(*lease))
        {
            s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
            return robobase_safe_build_status(hdr->seq, tx, tx_size);
        }

        if (s_seen_lease != 0U)
        {
            s_last_lease_age_ms = lease->linux_uptime_ms - s_last_linux_uptime_ms;
            if ((lease->lease_timeout_ms != 0U) && (s_last_lease_age_ms > lease->lease_timeout_ms))
            {
                s_fault_bits |= RB_FAULT_LINUX_TIMEOUT;
            }
        }
        else
        {
            s_last_lease_age_ms = 0U;
            s_seen_lease = 1U;
        }

        s_last_linux_seq = hdr->seq;
        s_last_linux_uptime_ms = lease->linux_uptime_ms;
        robobase_safe_update_motion(lease->linux_alive, lease->upstream_lease_valid, lease->driver_ok,
                                    lease->power_ok);
        if ((lease->lease_timeout_ms != 0U) && (s_last_lease_age_ms > lease->lease_timeout_ms))
        {
            s_fault_bits |= RB_FAULT_LINUX_TIMEOUT;
            s_motion_enable = 0U;
            if (s_latched_fault_bits != 0U)
            {
                s_state = RB_SAFE_FAULT_LATCHED;
            }
            else
            {
                s_state = RB_SAFE_STOP;
            }
        }
        return robobase_safe_build_status(hdr->seq, tx, tx_size);
    }

    if (hdr->msg_type == RB_SAFE_MSG_CLEAR_FAULT)
    {
        const struct rb_safe_clear_fault_msg *clear = (const struct rb_safe_clear_fault_msg *)payload;

        if (hdr->payload_len != sizeof(*clear))
        {
            s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
            return robobase_safe_build_status(hdr->seq, tx, tx_size);
        }

        if (clear->confirm == RB_SAFE_CLEAR_CONFIRM)
        {
            (void)robobase_safe_clear_faults(clear->clear_mask);
        }

        s_last_linux_seq = hdr->seq;
        return robobase_safe_build_status(hdr->seq, tx, tx_size);
    }

    s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
    return robobase_safe_build_status(hdr->seq, tx, tx_size);
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
        uint32_t rx_len;
        uint32_t tx_len;
        uint32_t dst;

        s_safety_loop_counter++;

        __disable_irq();
        if (s_rx_pending == 0U)
        {
            __DSB();
            __WFI();
            __enable_irq();
            continue;
        }

        rx_len = s_rx_len;
        dst = s_rx_src;
        tx_len = robobase_safe_handle_frame(s_rx_buf, rx_len, s_tx_buf, ROBOBASE_RPMSG_MAX_PAYLOAD);
        if (tx_len == 0U)
        {
            tx_len = rx_len;
            memcpy(s_tx_buf, s_rx_buf, tx_len);
        }
        s_rx_pending = 0U;
        __DMB();
        __enable_irq();

        (void)rpmsg_lite_send(rpmsg, ept, dst, (char *)s_tx_buf, tx_len, RL_BLOCK);
    }
}
