#include "robobase_safe_app.h"

#include "fsl_device_registers.h"
#include "robobase/rb_safety_proto.h"
#include "robobase_safety_inputs.h"

static uint32_t s_heartbeat_counter;
static uint32_t s_safety_loop_counter;
static uint32_t s_last_linux_seq;
static uint32_t s_last_linux_uptime_ms;
static uint32_t s_last_lease_m7_ms;
static uint32_t s_last_lease_age_ms;
static uint32_t s_current_lease_timeout_ms;
static volatile uint32_t s_m7_uptime_ms;
static volatile uint32_t s_safety_ticks_pending;
static uint8_t s_seen_lease;
static uint8_t s_latest_linux_alive;
static uint8_t s_latest_upstream_lease_valid;
static uint8_t s_latest_driver_ok = 1U;
static uint8_t s_latest_power_ok = 1U;
static uint8_t s_state = RB_SAFE_BOOT;
static uint8_t s_motion_enable;
static uint32_t s_fault_bits;
static uint32_t s_latched_fault_bits;
static robobase_safety_inputs_t s_inputs = {1U, 1U};

void robobase_safe_tick_1ms(void)
{
    s_m7_uptime_ms++;
    if (s_safety_ticks_pending < 1000U)
    {
        s_safety_ticks_pending++;
    }
}

static void robobase_safe_sample_inputs(void)
{
    robobase_safety_inputs_sample(&s_inputs);
}

static void robobase_safe_update_motion(uint8_t linux_alive, uint8_t upstream_lease_valid, uint8_t driver_ok,
                                        uint8_t power_ok)
{
    s_fault_bits = 0U;

    if (s_inputs.estop_nc_closed == 0U)
    {
        s_fault_bits |= RB_FAULT_ESTOP;
        s_latched_fault_bits |= RB_FAULT_ESTOP;
    }

    if (s_inputs.bumper_nc_closed == 0U)
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

    s_motion_enable = (uint8_t)((s_inputs.estop_nc_closed != 0U) && (s_inputs.bumper_nc_closed != 0U) &&
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

static void robobase_safe_check_watchdog(void)
{
    uint32_t age_ms;

    if ((s_seen_lease == 0U) || (s_current_lease_timeout_ms == 0U))
    {
        return;
    }

    age_ms = s_m7_uptime_ms - s_last_lease_m7_ms;
    s_last_lease_age_ms = age_ms;
    if (age_ms <= s_current_lease_timeout_ms)
    {
        return;
    }

    s_latest_linux_alive = 0U;
}

void robobase_safe_init(void)
{
    robobase_safety_inputs_init();
    robobase_safe_sample_inputs();
    robobase_safe_update_motion(s_latest_linux_alive, s_latest_upstream_lease_valid, s_latest_driver_ok,
                                s_latest_power_ok);
}

static void robobase_safe_run_1ms(void)
{
    s_safety_loop_counter++;
    robobase_safe_sample_inputs();
    robobase_safe_check_watchdog();
    robobase_safe_update_motion(s_latest_linux_alive, s_latest_upstream_lease_valid, s_latest_driver_ok,
                                s_latest_power_ok);
}

uint8_t robobase_safe_process_pending_tick(void)
{
    uint8_t have_tick = 0U;

    __disable_irq();
    if (s_safety_ticks_pending != 0U)
    {
        s_safety_ticks_pending--;
        have_tick = 1U;
    }
    __enable_irq();

    if (have_tick != 0U)
    {
        robobase_safe_run_1ms();
    }

    return have_tick;
}

static uint32_t robobase_safe_clear_faults(uint32_t clear_mask)
{
    uint32_t clearable = clear_mask;

    if (s_inputs.estop_nc_closed == 0U)
    {
        clearable &= ~RB_FAULT_ESTOP;
    }

    if (s_inputs.bumper_nc_closed == 0U)
    {
        clearable &= ~RB_FAULT_BUMPER;
    }

    s_latched_fault_bits &= ~clearable;
    robobase_safe_check_watchdog();
    robobase_safe_update_motion(s_latest_linux_alive, s_latest_upstream_lease_valid, s_latest_driver_ok,
                                s_latest_power_ok);
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
    status->m7_uptime_ms = s_m7_uptime_ms;
    status->heartbeat_counter = s_heartbeat_counter;
    status->state = s_state;
    status->motion_enable = s_motion_enable;
    status->estop_nc_closed = s_inputs.estop_nc_closed;
    status->bumper_nc_closed = s_inputs.bumper_nc_closed;
    status->fault_bits = s_fault_bits;
    status->latched_fault_bits = s_latched_fault_bits;
    status->last_linux_seq = s_last_linux_seq;
    status->last_lease_age_ms = s_last_lease_age_ms;
    status->safety_loop_counter = s_safety_loop_counter;
    status->reserved = 0U;

    return (uint32_t)(sizeof(*hdr) + sizeof(*status));
}

uint32_t robobase_safe_handle_frame(const uint8_t *rx, uint32_t rx_len, uint8_t *tx, uint32_t tx_size)
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

    if (hdr->msg_type == RB_SAFE_MSG_HELLO)
    {
        if (hdr->payload_len != 0U)
        {
            s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
        }
        s_last_linux_seq = hdr->seq;
        robobase_safe_check_watchdog();
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

        if (s_seen_lease == 0U)
        {
            s_seen_lease = 1U;
        }

        s_last_linux_seq = hdr->seq;
        s_last_linux_uptime_ms = lease->linux_uptime_ms;
        s_last_lease_m7_ms = s_m7_uptime_ms;
        s_last_lease_age_ms = 0U;
        s_current_lease_timeout_ms = lease->lease_timeout_ms;
        s_latest_linux_alive = lease->linux_alive;
        s_latest_upstream_lease_valid = lease->upstream_lease_valid;
        s_latest_driver_ok = lease->driver_ok;
        s_latest_power_ok = lease->power_ok;
        robobase_safe_update_motion(s_latest_linux_alive, s_latest_upstream_lease_valid, s_latest_driver_ok,
                                    s_latest_power_ok);
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

    if (hdr->msg_type == RB_SAFE_MSG_DEBUG_INPUTS)
    {
        const struct rb_safe_debug_inputs_msg *debug_inputs = (const struct rb_safe_debug_inputs_msg *)payload;

        if (hdr->payload_len != sizeof(*debug_inputs))
        {
            s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
            return robobase_safe_build_status(hdr->seq, tx, tx_size);
        }

        robobase_safety_inputs_set_debug_gpio_level(debug_inputs->estop_gpio_level,
                                                    debug_inputs->bumper_gpio_level,
                                                    debug_inputs->valid_mask);
        robobase_safe_run_1ms();
        s_last_linux_seq = hdr->seq;
        return robobase_safe_build_status(hdr->seq, tx, tx_size);
    }

    s_fault_bits |= RB_FAULT_PROTOCOL_ERROR;
    return robobase_safe_build_status(hdr->seq, tx, tx_size);
}
