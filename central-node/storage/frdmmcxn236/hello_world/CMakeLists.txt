cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(frdmmcxn236_hello_world LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/hello_world)

include(${SdkRootDirPath}/CMakeLists.txt)

# --- Application sources (this folder) ---
mcux_add_source(
    BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES hello_world.c
            lpspi_sdspi_host.c
            diskio.c
            logging.c
)
# Makes ffconf.h, logging.h etc. visible to all sources,
# plus the team's shared log_entry.h in the repo's common/ folder
mcux_add_include(
    BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
    INCLUDES .
             ../../../../common
)

# --- LPSPI driver ---
mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES drivers/lpspi/fsl_lpspi.c
)
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES drivers/lpspi
)

# --- RTC driver (only needed when FF_FS_NORTC is 0 in ffconf.h) ---
# DISABLED: the SDK's fsl_rtc driver does not match the MCXN236 RTC_Type
# registers and fails to compile. Uncomment once Person A has fixed or
# replaced it, and set FF_FS_NORTC to 0 in ffconf.h.
# mcux_add_source(
#     BASE_PATH ${SdkRootDirPath}
#     SOURCES drivers/rtc/fsl_rtc.c
# )
# mcux_add_include(
#     BASE_PATH ${SdkRootDirPath}
#     INCLUDES drivers/rtc
# )

# --- NXP generic SD-over-SPI driver ---
mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/sdmmc/sdspi/fsl_sdspi.c
)
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/sdmmc/sdspi
             middleware/sdmmc/common
)

# --- FATFS ---
mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES middleware/fatfs/source/ff.c
            middleware/fatfs/source/ffsystem.c
            middleware/fatfs/source/ffunicode.c
)
mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES middleware/fatfs/source
)

# --- SDSPI configuration switches (applied to fsl_sdspi.c as well) ---
target_compile_definitions(${MCUX_SDK_PROJECT_NAME} PRIVATE
    SDK_OS_BAREMETAL=1
    FSL_FEATURE_SDSPI_CARD_DETECTION_BY_GPIO=0
)

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/hello_world/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)