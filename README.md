# RoboBase M7 Source Dependencies

This directory is intended to become the `third_party/m7` submodule.

It contains source dependencies needed to build the MYD-JX8MP Cortex-M7 test
firmware. It should stay small enough to review and clone normally.

## Tracked Content

- `SDK_2_10_0_EVK-MIMX8MP/`: NXP EVK-MIMX8MP SDK source snapshot used as the
  base MCUXpresso SDK dependency for device headers, startup code, linker files,
  drivers, and the `robobase_m7_boot_only` application.
- `rpmsg-lite-minimal/`: minimal source copy extracted from the newer
  MCUXpresso SDK tree. It provides only the RPMsg-Lite and `remoteproc.h` files
  required by `robobase_m7_rpmsg_tty_echo.elf`.

## Intentionally Untracked

- `mcuxpresso-sdk/`: full west-downloaded MCUXpresso SDK workspace. It is about
  8.3 GB and is only used as an upstream source for extracting minimal files.
- `gcc-arm-none-eabi-*`: local Arm GCC toolchain.
- SDK/toolchain archives and generated `armgcc` build output directories.

## Build

```sh
export ARMGCC_DIR=/home/compile/workstation/project/codex/myplatform/third_party/m7/gcc-arm-none-eabi-7-2017-q4-major
cd /home/compile/workstation/project/codex/myplatform/third_party/m7/SDK_2_10_0_EVK-MIMX8MP/boards/evkmimx8mp/demo_apps/robobase_m7_boot_only/armgcc
./build_debug.sh
```

The RPMsg firmware now builds from `rpmsg-lite-minimal/`. The full
`mcuxpresso-sdk/` workspace is not required after the extraction.

## Submodule Rule

When converting this directory into a real Git submodule, commit only the
tracked content above. Do not commit the 8.3 GB west workspace, local toolchain,
archives, or generated build output.
