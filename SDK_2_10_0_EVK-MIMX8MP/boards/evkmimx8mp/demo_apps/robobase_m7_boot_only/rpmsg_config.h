/*
 * Minimal RPMsg-Lite configuration for the RoboBase M7 tty echo firmware.
 *
 * The shared-memory layout matches the Linux reserved-memory nodes added for
 * i.MX8MP CM7 remoteproc:
 *   vring0: 0x55000000, size 0x8000
 *   vring1: 0x55008000, size 0x8000
 */

#ifndef RPMSG_CONFIG_H_
#define RPMSG_CONFIG_H_

#define RL_MS_PER_INTERVAL (1)

/* Linux rpmsg uses 512-byte buffers including the 16-byte rpmsg header. */
#define RL_BUFFER_PAYLOAD_SIZE (496U)

/* Keep this in sync with robobase_m7_rsc_table.c and Linux vring setup. */
#define RL_BUFFER_COUNT (256U)

#define RL_API_HAS_ZEROCOPY (1)
#define RL_USE_STATIC_API (1)
#define RL_CLEAR_USED_BUFFERS (0)
#define RL_USE_MCMGR_IPC_ISR_HANDLER (0)
#define RL_USE_ENVIRONMENT_CONTEXT (0)
#define RL_DEBUG_CHECK_BUFFERS (0)
#define RL_USE_DCACHE (0)

#endif /* RPMSG_CONFIG_H_ */
