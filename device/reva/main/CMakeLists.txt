# The core and the portable firmware with the decimal library cross-built for the S3, and the
# ULP-RISC-V program that toggles EXTCOMIN (ulp/main.c).
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(BID_LIB "${REPO}/build/intel-esp32s3/libbid.a")
set(BID_INC "${REPO}/third_party/intel-dfp/esp32s3/IntelRDFPMathLib20U5/LIBRARY/src")
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB CORE_SRCS "${REPO}/core/*.c")
else()
    file(GLOB CORE_SRCS CONFIGURE_DEPENDS "${REPO}/core/*.c")
endif()
set(FW "${REPO}/firmware")
# STU_WASM: firmware/wasm/wasm_app.c (the bridge to WAMR) and the wamr component (device/CMakeLists.txt)
if("$ENV{STU_WASM}" STREQUAL "1")
    if(NOT EXISTS "${REPO}/build/wamr/.pin")
        message(FATAL_ERROR "STU_WASM=1 but no ${REPO}/build/wamr: run scripts/fetch-wamr.sh")
    endif()
    set(WASM_SRCS "${FW}/wasm/wasm_app.c")
    set(WASM_INC "${FW}/wasm")
    set(WASM_REQUIRES wamr)
endif()
# The games (games.list), exported and described by scripts/gen_games.py before the build
if(NOT EXISTS "${REPO}/build/games/games.cmake")
    message(FATAL_ERROR "no ${REPO}/build/games/games.cmake: run python3 scripts/gen_games.py games.list build/games")
endif()
include("${REPO}/build/games/games.cmake")
# Casimir (CAS 004): build/casim from scripts/fetch-casim.sh, its file under the frame guard, the core's CAS step on
set(CASIM "${REPO}/build/casim")
if(NOT EXISTS "${CASIM}/.pin")
    message(FATAL_ERROR "no ${CASIM}: run scripts/fetch-casim.sh")
endif()
if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties("${CASIM}/casim.c" PROPERTIES COMPILE_OPTIONS "-Werror=frame-larger-than=1024"
                                OBJECT_DEPENDS "${CASIM}/.pin")
endif()

idf_component_register(
    SRCS ${WASM_SRCS} reva_main.c ${CORE_SRCS} "${FW}/screen.c" "${FW}/font5x7.c" "${FW}/draw.c" "${FW}/app.c" "${FW}/apps.c" "${FW}/keymap.c"
         "${FW}/keys.c" "${FW}/power.c" "${FW}/store.c" "${FW}/ls027.c" "${FW}/gauge.c" "${FW}/pins.c" "${FW}/piezo.c" "${FW}/settings.c"
         ${GAMES_SRCS} "${CASIM}/casim.c"
    EMBED_FILES ${GAMES_EMBED}
    INCLUDE_DIRS . "${REPO}/core" "${FW}" ${WASM_INC} "${REPO}/device" "${REPO}/build/games" "${CASIM}"
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_psram esp_partition esp_timer ulp esp_hw_support ${WASM_REQUIRES})

set(ulp_app_name ulp_${COMPONENT_NAME})
ulp_embed_binary(${ulp_app_name} "ulp/main.c" "reva_main.c")

if(NOT EXISTS "${BID_LIB}")
    message(FATAL_ERROR "no ${BID_LIB}: run TARGET=esp32s3 scripts/fetch-intel-dfp.sh")
endif()
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${BID_INC}")
# SYS's VER (unit 027): the commit, with +dirty for an uncommitted tree, regenerated at every build
# (abacus #4238); the header is rewritten only when its text changes
set(AB_COMMIT_H "${CMAKE_CURRENT_BINARY_DIR}/ab_commit.h")
add_custom_target(ab_commit ALL
    COMMAND ${CMAKE_COMMAND} -DREPO=${REPO} -DOUT=${AB_COMMIT_H} -P ${REPO}/scripts/gen_commit.cmake
    BYPRODUCTS ${AB_COMMIT_H} VERBATIM)
add_dependencies(${COMPONENT_LIB} ab_commit)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE AB_HAVE_COMMIT_H=1 AB_CASIM=1)
# unit 053: REVA_HEADLESS (device/reva/headless.sh) runs this main on the stand-in's S3, driving none of Rev A's pins,
# with the benches; AB_BENCH alone adds the benches (dev_bench.h) to a Rev A image for bring-up timing. Neither is in
# the default image (decision 68).
if(REVA_HEADLESS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE REVA_HEADLESS=1 AB_BENCH=1)
elseif(AB_BENCH)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE AB_BENCH=1)
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
set_source_files_properties("${REPO}/core/arena.c" PROPERTIES COMPILE_OPTIONS "-include;${REPO}/device/pool_attr.h")
if("$ENV{STU_WASM}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE STU_WASM=1)
    set_source_files_properties("${FW}/wasm/wasm_app.c" PROPERTIES COMPILE_OPTIONS "-Werror=frame-larger-than=1024")
endif()
