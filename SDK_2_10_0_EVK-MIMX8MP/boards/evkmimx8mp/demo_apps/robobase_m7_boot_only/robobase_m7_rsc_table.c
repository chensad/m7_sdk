/*
 * Minimal resource table for a Linux-controlled i.MX8MP CM7 RPMsg endpoint.
 *
 * Linux remoteproc reads this ELF section before starting M7. The vring
 * addresses must match the board device tree reserved-memory nodes.
 */

#include <stddef.h>
#include <stdint.h>

#include "remoteproc.h"
#include "rpmsg_lite.h"
#include "rpmsg_platform.h"

#define ROBOBASE_VDEV0_VRING_BASE (0x55000000U)
#define ROBOBASE_NUM_VRINGS (2U)
#define ROBOBASE_NUM_RSC_ENTRIES (1U)
#define ROBOBASE_RSC_VDEV_FEATURE_NS (1U)

METAL_PACKED_BEGIN
struct robobase_remote_resource_table
{
    uint32_t version;
    uint32_t num;
    uint32_t reserved[2];
    uint32_t offset[ROBOBASE_NUM_RSC_ENTRIES];
    struct fw_rsc_vdev user_vdev;
    struct fw_rsc_vdev_vring user_vring0;
    struct fw_rsc_vdev_vring user_vring1;
} METAL_PACKED_END;

#if defined(__GNUC__)
__attribute__((section(".resource_table"), used))
#else
#error Compiler not supported
#endif
const struct robobase_remote_resource_table resources = {
    1,
    ROBOBASE_NUM_RSC_ENTRIES,
    {0, 0},
    {offsetof(struct robobase_remote_resource_table, user_vdev)},
    {
        RSC_VDEV,
        7,
        0,
        ROBOBASE_RSC_VDEV_FEATURE_NS,
        0,
        0,
        0,
        ROBOBASE_NUM_VRINGS,
        {0, 0},
    },
    {ROBOBASE_VDEV0_VRING_BASE, VRING_ALIGN, RL_BUFFER_COUNT, 0, 0},
    {ROBOBASE_VDEV0_VRING_BASE + VRING_SIZE, VRING_ALIGN, RL_BUFFER_COUNT, 1, 0},
};
