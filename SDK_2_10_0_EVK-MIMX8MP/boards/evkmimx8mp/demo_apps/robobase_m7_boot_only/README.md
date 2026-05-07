# robobase_m7_boot_only

Minimal i.MX8MP Cortex-M7 firmware for Linux remoteproc bring-up on the MYIR
MYD-JX8MP board.

This project is copied from the NXP `hello_world` example, but it deliberately
does not call:

- `BOARD_RdcInit()`
- `BOARD_BootClockRUN()`
- `BOARD_InitBootPins()`
- `BOARD_InitDebugConsole()`
- UART, I2C, SPI, CAN, SAI, SDMA, GPIO, or AudioMix initialization

The first validation target is only:

- Linux does not hang after `echo start > /sys/class/remoteproc/remoteproc0/state`
- `remoteproc0/state` becomes `running`

The firmware body only executes `wfi` in a loop. It does not run a busy loop or
write a heartbeat variable, so it should not create load on shared buses or TCM.

Build:

```sh
export ARMGCC_DIR=/home/compile/workstation/project/codex/myplatform/third_party/m7/gcc-arm-none-eabi-7-2017-q4-major
cd /home/compile/workstation/project/codex/myplatform/third_party/m7/SDK_2_10_0_EVK-MIMX8MP/boards/evkmimx8mp/demo_apps/robobase_m7_boot_only/armgcc
./build_debug.sh
```

Output ELF:

```text
debug/robobase_m7_boot_only.elf
```

The same `armgcc` project also builds a minimal RPMsg tty echo firmware:

```text
debug/robobase_m7_rpmsg_tty_echo.elf
debug/robobase_m7_rpmsg_tty_echo.bin
```

The RPMsg target uses the minimal source copy in:

```text
third_party/m7/rpmsg-lite-minimal
```

It does not require the full west-downloaded `third_party/m7/mcuxpresso-sdk`
workspace after that source copy has been extracted.

This RPMsg firmware is intentionally smaller than the NXP FreeRTOS examples:

- it does not call `BOARD_InitHardware()`
- it does not initialize UART or use `PRINTF`
- it does not change pinmux, RDC, AudioMix, or the clock tree
- it uses the RPMsg-Lite bare-metal environment
- it uses static RPMsg-Lite contexts, not FreeRTOS queues
- it only initializes MU and announces `rpmsg-virtual-tty-channel-1`
- it preserves byte echo mode while also accepting first-version RoboBase
  safety protocol frames

The shared safety protocol ABI is:

```text
platform/common/include/robobase/rb_safety_proto.h
```

Version 0.1 defines Linux-to-M7 `LEASE`, M7-to-Linux `STATUS`, and
Linux-to-M7 `CLEAR_FAULT` frames. The initial state machine supports `BOOT`,
`STANDBY`, `ARMED`, `RUNNING`, `SAFE_STOP`, and `FAULT_LATCHED`.

The RPMsg resource table uses the Linux reserved-memory layout:

```text
vring0:     0x55000000, size 0x8000
vring1:     0x55008000, size 0x8000
rsc_table:  0x550ff000, size 0x1000
vdevbuffer: 0x55400000, size 0x100000
```

Deploy to the board:

```sh
scp -O debug/robobase_m7_boot_only.elf root@192.168.1.8:/lib/firmware/
scp -O debug/robobase_m7_rpmsg_tty_echo.elf root@192.168.1.8:/lib/firmware/
```

Start from Linux:

```sh
R=/sys/class/remoteproc/remoteproc0
echo robobase_m7_boot_only.elf > "$R/firmware"
echo start > "$R/state"
cat "$R/state"
dmesg | grep -Ei 'remoteproc|imx-rproc|rpmsg'
```

If this firmware still freezes Linux, do not continue with RPMsg yet. Check the
remoteproc DTS, TCM mapping, reset flow, and reserved memory first.

## Minimal RPMsg TTY Echo Validation

Always start this firmware from a clean board boot while `remoteproc0` is still
`offline`. Do not test it immediately after a failed or forced M7 stop, because
the previous firmware can leave the CM7/ATF/MU state dirty.

Load the Linux tty RPMsg driver:

```sh
modprobe imx_rpmsg_tty
lsmod | grep -Ei 'rpmsg|imx_rpmsg_tty'
```

Start the M7 firmware:

```sh
R=/sys/class/remoteproc/remoteproc0
cat "$R/state"
echo robobase_m7_rpmsg_tty_echo.elf > "$R/firmware"
echo start > "$R/state"
cat "$R/state"
```

Check that Linux sees the RPMsg transport and the announced tty channel:

```sh
dmesg | grep -Ei 'remoteproc|virtio_rpmsg|rpmsg|ttyRPMSG|rpmsg-virtual' | tail -120
for d in /sys/bus/rpmsg/devices/*; do
    echo "== $d =="
    [ -e "$d/name" ] && cat "$d/name"
    [ -e "$d/src" ] && cat "$d/src"
    [ -e "$d/dst" ] && cat "$d/dst"
    readlink "$d/driver" 2>/dev/null
done
ls -l /dev | grep -Ei 'rpmsg|ttyRPMSG'
```

Expected result:

```text
virtio_rpmsg_bus virtio0: rpmsg host is online
rpmsg-virtual-tty-channel-1
/dev/ttyRPMSG*
```

Echo test:

```sh
TTY=/dev/ttyRPMSG0
stty -F "$TTY" raw -echo -icanon min 0 time 10
cat "$TTY" &
CATPID=$!
printf 'robobase-test\n' > "$TTY"
sleep 1
kill "$CATPID"
```

Safety protocol test with the Linux user-space tool:

```sh
robobase-rpmsg-test --safety -d /dev/ttyRPMSG30 -n 10
robobase-rpmsg-test --safety -d /dev/ttyRPMSG30 --upstream-invalid
robobase-rpmsg-test --safety -d /dev/ttyRPMSG30 --clear-fault 0xffffffff
```

The v0.1 timeout path is not a final safety watchdog. It is only a bring-up
approximation based on observed Linux lease timing; the next firmware step is
to add an M7 timer tick so link-loss can be detected without receiving another
RPMsg frame.

If `/sys/bus/rpmsg/devices` only contains `rpmsg_ctrl` and `rpmsg_ns`, the
Linux transport is up but the M7 firmware did not announce the tty data channel.
In that case, capture:

```sh
cat /sys/class/remoteproc/remoteproc0/state
dmesg | grep -Ei 'remoteproc|virtio_rpmsg|rpmsg|ttyRPMSG|imx-rproc' | tail -200
find /sys/bus/rpmsg -maxdepth 3 -type f -o -type l
```
