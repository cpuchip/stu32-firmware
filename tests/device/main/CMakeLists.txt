# The core, make test's runners and the generated bundle, linked with the decimal library
# cross-built for this target (scripts/fetch-intel-dfp.sh with TARGET set).
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
idf_build_get_property(target IDF_TARGET)
set(BUNDLE_C "${CMAKE_CURRENT_LIST_DIR}/generated/bundle.c")
set(BID_LIB "${REPO}/build/intel-${target}/libbid.a")
set(BID_INC "${REPO}/third_party/intel-dfp/${target}/IntelRDFPMathLib20U5/LIBRARY/src")
# A new core file joins without a clean (CONFIGURE_DEPENDS), except in the script-mode pass ESP-IDF
# makes to read requirements, where that keyword is refused.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB CORE_SRCS "${REPO}/core/*.c")
else()
    file(GLOB CORE_SRCS CONFIGURE_DEPENDS "${REPO}/core/*.c")
endif()

# Casimir (CAS 004), as the stand-in builds it: build/casim from scripts/fetch-casim.sh, under the frame guard
set(CASIM "${REPO}/build/casim")
if(NOT EXISTS "${CASIM}/.pin")
    message(FATAL_ERROR "no ${CASIM}: run scripts/fetch-casim.sh")
endif()
if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties("${CASIM}/casim.c" PROPERTIES COMPILE_OPTIONS "-Werror=frame-larger-than=1024")
endif()

# The S3's PSRAM needs its component in the build (set(COMPONENTS main) brings only main's requirements);
# naming one requirement ends main's implicit all, so the timer and partition components are named too.
# The C6 keeps the implicit set.
set(DEV_REQS)
if(target STREQUAL "esp32s3")
    set(DEV_REQS PRIV_REQUIRES esp_psram esp_timer esp_partition)
endif()

idf_component_register(
    ${DEV_REQS}
    SRCS device_main.c "${BUNDLE_C}" ${CORE_SRCS} "${CASIM}/casim.c"
         "${REPO}/tests/vectors.c" "${REPO}/tests/fmt_vectors.c" "${REPO}/tests/diff_test.c"
         "${REPO}/tests/dref.c" "${REPO}/tests/abn_test.c" "${REPO}/tests/xnum_test.c"
         "${REPO}/firmware/store.c" "${REPO}/firmware/draw.c" "${REPO}/firmware/font5x7.c" "${REPO}/tests/draw_test.c"
    INCLUDE_DIRS . "${REPO}/core" "${REPO}/tests" "${REPO}/firmware" "${CASIM}")

if(NOT EXISTS "${BUNDLE_C}")
    message(FATAL_ERROR "no ${BUNDLE_C}: run tests/device/run.sh (it runs bundle.py first)")
endif()
if(NOT EXISTS "${BID_LIB}")
    message(FATAL_ERROR "no ${BID_LIB}: run TARGET=${target} scripts/fetch-intel-dfp.sh")
endif()

target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${BID_INC}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE AB_DEVICE TEST_STACK=40960 AB_CASIM=1
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
set_source_files_properties("${REPO}/tests/abn_test.c" PROPERTIES COMPILE_DEFINITIONS "main=abn_test_main")
set_source_files_properties("${REPO}/tests/xnum_test.c" PROPERTIES COMPILE_DEFINITIONS "main=xnum_test_main")
set_source_files_properties("${REPO}/tests/draw_test.c" PROPERTIES COMPILE_DEFINITIONS "main=draw_test_main")   # apps.h v1.2: sdk/draw on the ILP32 chip
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
# On the S3 the pools go to PSRAM, as on the stand-in (sdkconfig.defaults.esp32s3); the C6 has none
if(target STREQUAL "esp32s3")
    set_source_files_properties("${REPO}/core/arena.c" PROPERTIES COMPILE_OPTIONS "-include;${REPO}/device/pool_attr.h")
endif()
