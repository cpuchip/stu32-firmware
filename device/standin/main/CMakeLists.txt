# The core and the portable firmware, linked with the decimal library cross-built for the S3
# (scripts/fetch-intel-dfp.sh with TARGET=esp32s3, run by device/standin/run.sh).
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(BID_LIB "${REPO}/build/intel-esp32s3/libbid.a")
set(BID_INC "${REPO}/third_party/intel-dfp/esp32s3/IntelRDFPMathLib20U5/LIBRARY/src")
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB CORE_SRCS "${REPO}/core/*.c")
else()
    file(GLOB CORE_SRCS CONFIGURE_DEPENDS "${REPO}/core/*.c")
endif()
set(FW "${REPO}/firmware")
# Babal (stu32-arcade at ARCADE_PIN), exported by scripts/fetch-arcade.sh before the build
set(ARCADE "${REPO}/build/arcade")
if(NOT EXISTS "${ARCADE}/.pin")
    message(FATAL_ERROR "no ${ARCADE}: run scripts/fetch-arcade.sh")
endif()
file(GLOB ARCADE_SRCS "${ARCADE}/babal/*.c")
list(APPEND ARCADE_SRCS "${ARCADE}/app/babal_app.c" "${ARCADE}/app/render.c")
if(NOT CMAKE_SCRIPT_MODE_FILE)  # not allowed while IDF expands requirements in script mode
    set_source_files_properties(${ARCADE_SRCS} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif()

idf_component_register(
    SRCS standin_main.c ${CORE_SRCS} "${FW}/screen.c" "${FW}/font5x7.c" "${FW}/app.c" "${FW}/apps.c" "${FW}/keymap.c"
         "${FW}/keys.c" "${FW}/power.c" "${FW}/standin.c" "${FW}/store.c" "${FW}/ls027.c" "${FW}/settings.c" "${FW}/piezo.c"
         ${ARCADE_SRCS}
    INCLUDE_DIRS . "${REPO}/core" "${FW}" "${REPO}/device" "${ARCADE}/babal" "${ARCADE}/app"
    REQUIRES esp_lcd esp_driver_i2c esp_driver_gpio esp_driver_uart vfs esp_psram esp_partition esp_timer)

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
target_compile_definitions(${COMPONENT_LIB} PRIVATE AB_HAVE_COMMIT_H=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
set_source_files_properties("${REPO}/core/arena.c" PROPERTIES COMPILE_OPTIONS "-include;${REPO}/device/pool_attr.h")
