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
- the RPMsg transport, safety state machine, and safety input sampling are kept
  in separate source files so the first GPIO backend can be added without
  rewriting the protocol path
- GPIO sampling and Linux lease watchdog checks run from a fixed 1 ms safety
  tick driven by SysTick pending work, not from the RPMsg receive cadence
- RPMsg handlers update command/lease inputs and return the latest safety
  snapshot; they do not own the GPIO polling loop

The shared safety protocol ABI is:

```text
platform/common/include/robobase/rb_safety_proto.h
```

Version 0.1 defines Linux-to-M7 `LEASE`, M7-to-Linux `STATUS`, and
Linux-to-M7 `CLEAR_FAULT` frames. The initial state machine supports `BOOT`,
`STANDBY`, `ARMED`, `RUNNING`, `SAFE_STOP`, and `FAULT_LATCHED`.

Safety input sampling is implemented in:

```text
robobase_safety_inputs.c
```

The M7 software safety authorization output is implemented in:

```text
robobase_safety_outputs.c
```

M7 configures both pads as real GPIO5 inputs and samples the GPIO pad status.
For no-device bench testing, the Linux test tool can send a debug frame that
changes each pad's internal pull-up/pull-down bias. This gives a real GPIO
sample without attaching the mushroom switch or bumper yet:

```sh
robobase-rpmsg-test --estop-gpio-high --safety -d /dev/ttyRPMSG30 -n 1
robobase-rpmsg-test --estop-gpio-low --bumper-gpio-low --clear-fault 0xffffffff -d /dev/ttyRPMSG30
robobase-rpmsg-test --bumper-gpio-high --safety -d /dev/ttyRPMSG30 -n 1
robobase-rpmsg-test --bumper-gpio-low --estop-gpio-low --clear-fault 0xffffffff -d /dev/ttyRPMSG30
```

The debug levels follow the planned NC wiring:

```text
GPIO low  = pull-down, NC contact closed to GND = safe
GPIO high = pull-up, NC contact open/fault
```

The firmware defaults both inputs to internal pull-up. With no switches attached,
the first status query should therefore report both inputs open/fault until the
test tool biases them low.

The intended first hardware mapping is:

```text
E-stop NC auxiliary contact: J25 pin 23, ECSPI2_SCLK_3V3, GPIO5_IO10
Bumper/microswitch NC:       J25 pin 21, ECSPI2_MISO_3V3, GPIO5_IO12
M7 safety_allow output:      J25 pin 19, ECSPI2_MOSI_3V3, GPIO5_IO11
```

The normalized safety input value remains `1 = NC closed, safe` and
`0 = NC open, fault`.
The `safety_allow` output is active high and defaults low. It is intended to
drive one input of the external AND gate; the other input should come from the
hardwired E-stop/bumper NC safety chain. The AND output then drives the
BTS7960 `R_EN`/`L_EN` enable node.

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

M7 local watchdog test:

```sh
robobase-rpmsg-test --safety -d /dev/ttyRPMSG30 --lease-timeout-ms 200 -n 1
sleep 1
robobase-rpmsg-test --query-status -d /dev/ttyRPMSG30
```

The query uses a `HELLO` frame, so it reads M7 `STATUS` without refreshing the
Linux lease. After the timeout expires, expect `state=SAFE_STOP`, `motion=0`,
and `fault=0x00000004`.

If `/sys/bus/rpmsg/devices` only contains `rpmsg_ctrl` and `rpmsg_ns`, the
Linux transport is up but the M7 firmware did not announce the tty data channel.
In that case, capture:

```sh
cat /sys/class/remoteproc/remoteproc0/state
dmesg | grep -Ei 'remoteproc|virtio_rpmsg|rpmsg|ttyRPMSG|imx-rproc' | tail -200
find /sys/bus/rpmsg -maxdepth 3 -type f -o -type l
```
# 2026-09-19: GPIO MMIO memory attributes

The RPMsg safety firmware now configures `0x30000000..0x30ffffff` as
Device/shareable/execute-never memory before safety GPIO initialization.
The highest implemented MPU region is reserved for this purpose. Other memory
attributes and cache state are left unchanged; do not overwrite this region
when adding another MPU configuration.

Without this setup, board diagnostics observed writes disturbing adjacent
IOMUX/GPIO registers: J25 pin19 stayed high despite `motion=0`. The MPU
diagnostic candidate passed user-reported switch, latch and lease-timeout
checks. The formal image still needs deployment and a final board regression.
See `myplatform/docs/boards/myir_imx8m_plus_m7_gpio19_mpu_debug.md` in the outer
project for evidence, hashes, deployment and validation steps.
