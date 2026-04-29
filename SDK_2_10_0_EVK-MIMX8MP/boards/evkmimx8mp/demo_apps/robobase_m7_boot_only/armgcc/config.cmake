# config to select component, the format is CONFIG_USE_${component}
#
# This boot-only firmware intentionally does not select UART, serial manager,
# clock, RDC, pinmux, or peripheral drivers. It must not touch Linux-owned SoC
# resources when started by remoteproc.
