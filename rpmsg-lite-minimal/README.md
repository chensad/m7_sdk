# Minimal RPMsg-Lite Source Copy

This directory is a curated source copy for the MYD-JX8MP minimal Cortex-M7
RPMsg firmware. It is not a full MCUXpresso SDK checkout.

## Upstream Source

- Full workspace: `third_party/m7/mcuxpresso-sdk/mcuxsdk`
- MCUXpresso SDK manifest requested by the project: `v25.09.00`
- `mcuxsdk` commit used during extraction:
  `e570f283a05a8cc291ac1165b405168250b9717a`
- `middleware/multicore/rpmsg-lite` commit used during extraction:
  `98f770d0c915397bad61cb152f381cafcb5af254`

## Included Files

Only files needed by
`SDK_2_10_0_EVK-MIMX8MP/boards/evkmimx8mp/demo_apps/robobase_m7_boot_only`
are copied:

- RPMsg-Lite core sources: `rpmsg_lite.c`, `rpmsg_ns.c`
- Bare-metal environment port: `rpmsg_env_bm.c`
- i.MX8MP M7 platform port: `rpmsg_platform.c`
- Virtqueue and linked-list helpers: `virtqueue.c`, `llist.c`
- Matching RPMsg-Lite headers for those sources
- `remoteproc/remoteproc.h` for the resource table definitions
- Upstream `LICENSE` and `README.md`

Excluded content includes tests, Zephyr integration, FreeRTOS ports, generated
metadata, `.git` directories, and documentation assets.

## Update Procedure

If RPMsg-Lite needs to be refreshed later, update the full local
`mcuxpresso-sdk/` workspace outside version control, copy only the files listed
above into this directory, rebuild `robobase_m7_rpmsg_tty_echo.elf`, and update
the commit hashes in this README.
