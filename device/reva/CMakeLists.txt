# Rev A's firmware (unit 018): the calculator on its own board. Built in the pinned image with
# device/reva/build.sh; not yet run on a board (Rev A is being ordered).
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
# STU_WASM=1 in the environment (decisions 67, 68; off by default): WAMR from scripts/fetch-wamr.sh, AOT only
if("$ENV{STU_WASM}" STREQUAL "1")
    get_filename_component(WAMR_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    set(EXTRA_COMPONENT_DIRS "${WAMR_REPO_ROOT}/build/wamr/build-scripts/esp-idf")
    if(NOT DEFINED SDKCONFIG_DEFAULTS)   # appended to, so a caller's own defaults files are kept (soroban #5542)
        set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
    endif()
    list(APPEND SDKCONFIG_DEFAULTS "${WAMR_REPO_ROOT}/device/sdkconfig.wasm")
    set(WAMR_BUILD_QUICK_AOT_ENTRY 0)  # its 768 B table is sorted at init, so it would sit in internal DRAM (decision 68)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if("$ENV{STU_WASM}" STREQUAL "1")
    idf_build_set_property(WAMR_NO_LWIP 1)    # no libc-wasi, so no lwip and its VFS and console (WAMR patch 0005)
endif()
project(abacus_reva)
