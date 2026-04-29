# CROSS COMPILER SETTING
SET(CMAKE_SYSTEM_NAME Generic)
CMAKE_MINIMUM_REQUIRED (VERSION 3.10.0)

# THE VERSION NUMBER
SET (Tutorial_VERSION_MAJOR 1)
SET (Tutorial_VERSION_MINOR 0)

# ENABLE ASM
ENABLE_LANGUAGE(ASM)

SET(CMAKE_STATIC_LIBRARY_PREFIX)
SET(CMAKE_STATIC_LIBRARY_SUFFIX)

SET(CMAKE_EXECUTABLE_LIBRARY_PREFIX)
SET(CMAKE_EXECUTABLE_LIBRARY_SUFFIX)

# CURRENT DIRECTORY
SET(ProjDirPath ${CMAKE_CURRENT_SOURCE_DIR})

SET(EXECUTABLE_OUTPUT_PATH ${ProjDirPath}/${CMAKE_BUILD_TYPE})
SET(LIBRARY_OUTPUT_PATH ${ProjDirPath}/${CMAKE_BUILD_TYPE})


project(robobase_m7_boot_only)

set(MCUX_SDK_PROJECT_NAME robobase_m7_boot_only.elf)
set(ROBOBASE_RPMSG_PROJECT_NAME robobase_m7_rpmsg_tty_echo.elf)
set(OLD_MCUX_SDK_ROOT ${ProjDirPath}/../../../../../)
set(M7_THIRD_PARTY_ROOT ${ProjDirPath}/../../../../../../)
set(RPMSG_LITE_DIR ${M7_THIRD_PARTY_ROOT}/rpmsg-lite-minimal/rpmsg-lite)
set(REMOTEPROC_DIR ${M7_THIRD_PARTY_ROOT}/rpmsg-lite-minimal/remoteproc)

include(${ProjDirPath}/flags.cmake)

include(${ProjDirPath}/config.cmake)

add_executable(${MCUX_SDK_PROJECT_NAME} 
"${ProjDirPath}/../robobase_m7_boot_only.c"
"${ProjDirPath}/../empty_rsc_table.c"
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE
    ${ProjDirPath}/..
)

set(CMAKE_MODULE_PATH
    ${ProjDirPath}/../../../../../devices/MIMX8ML8
    ${ProjDirPath}/../../../../../CMSIS/Core/Include
)

# include modules
include(device_MIMX8ML8_CMSIS_MIMX8ML8)

include(device_MIMX8ML8_startup_MIMX8ML8)

include(CMSIS_Include_core_cm_MIMX8ML8)

include(device_MIMX8ML8_system_MIMX8ML8)


TARGET_LINK_LIBRARIES(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--start-group)

target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE m)

target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE c)

target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE gcc)

target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE nosys)

TARGET_LINK_LIBRARIES(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--end-group)

ADD_CUSTOM_COMMAND(TARGET ${MCUX_SDK_PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY}
-Obinary ${EXECUTABLE_OUTPUT_PATH}/${MCUX_SDK_PROJECT_NAME} ${EXECUTABLE_OUTPUT_PATH}/robobase_m7_boot_only.bin)

add_executable(${ROBOBASE_RPMSG_PROJECT_NAME}
"${ProjDirPath}/../robobase_m7_rpmsg_tty_echo.c"
"${ProjDirPath}/../robobase_m7_rsc_table.c"
"${RPMSG_LITE_DIR}/lib/rpmsg_lite/rpmsg_lite.c"
"${RPMSG_LITE_DIR}/lib/rpmsg_lite/rpmsg_ns.c"
"${RPMSG_LITE_DIR}/lib/rpmsg_lite/porting/environment/rpmsg_env_bm.c"
"${RPMSG_LITE_DIR}/lib/rpmsg_lite/porting/platform/imx8mp_m7/rpmsg_platform.c"
"${RPMSG_LITE_DIR}/lib/virtio/virtqueue.c"
"${RPMSG_LITE_DIR}/lib/common/llist.c"
"${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/drivers/fsl_common.c"
"${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/drivers/fsl_common_arm.c"
"${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/drivers/fsl_mu.c"
"${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/gcc/startup_MIMX8ML8_cm7.S"
"${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/system_MIMX8ML8_cm7.c"
)

target_compile_definitions(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE
    SDK_DEBUGCONSOLE=2
    FSL_SDK_DISABLE_DRIVER_CLOCK_CONTROL=1
)

target_include_directories(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE
    ${ProjDirPath}/..
    ${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8
    ${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/drivers
    ${OLD_MCUX_SDK_ROOT}/devices/MIMX8ML8/utilities/debug_console_lite
    ${OLD_MCUX_SDK_ROOT}/CMSIS/Core/Include
    ${RPMSG_LITE_DIR}/lib/include
    ${RPMSG_LITE_DIR}/lib/include/environment/bm
    ${RPMSG_LITE_DIR}/lib/include/platform/imx8mp_m7
    ${REMOTEPROC_DIR}
)

TARGET_LINK_LIBRARIES(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE -Wl,--start-group)

target_link_libraries(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE m)

target_link_libraries(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE c)

target_link_libraries(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE gcc)

target_link_libraries(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE nosys)

TARGET_LINK_LIBRARIES(${ROBOBASE_RPMSG_PROJECT_NAME} PRIVATE -Wl,--end-group)

ADD_CUSTOM_COMMAND(TARGET ${ROBOBASE_RPMSG_PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY}
-Obinary ${EXECUTABLE_OUTPUT_PATH}/${ROBOBASE_RPMSG_PROJECT_NAME} ${EXECUTABLE_OUTPUT_PATH}/robobase_m7_rpmsg_tty_echo.bin)
