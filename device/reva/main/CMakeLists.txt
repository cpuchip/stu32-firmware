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

idf_component_register(
    SRCS reva_main.c ${CORE_SRCS} "${FW}/screen.c" "${FW}/font5x7.c" "${FW}/app.c" "${FW}/keymap.c"
         "${FW}/keys.c" "${FW}/power.c" "${FW}/store.c" "${FW}/ls027.c" "${FW}/gauge.c" "${FW}/pins.c" "${FW}/piezo.c"
    INCLUDE_DIRS . "${REPO}/core" "${FW}"
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_psram esp_partition esp_timer ulp esp_hw_support)

set(ulp_app_name ulp_${COMPONENT_NAME})
ulp_embed_binary(${ulp_app_name} "ulp/main.c" "reva_main.c")

if(NOT EXISTS "${BID_LIB}")
    message(FATAL_ERROR "no ${BID_LIB}: run TARGET=esp32s3 scripts/fetch-intel-dfp.sh")
endif()
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${BID_INC}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
set_source_files_properties("${REPO}/core/arena.c" PROPERTIES COMPILE_OPTIONS "-include;${REPO}/device/pool_attr.h")
