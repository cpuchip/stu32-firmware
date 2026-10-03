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

idf_component_register(
    SRCS device_main.c "${BUNDLE_C}" ${CORE_SRCS}
         "${REPO}/tests/vectors.c" "${REPO}/tests/fmt_vectors.c" "${REPO}/tests/diff_test.c"
         "${REPO}/tests/dref.c" "${REPO}/tests/abn_test.c" "${REPO}/tests/xnum_test.c"
         "${REPO}/firmware/store.c"
    INCLUDE_DIRS . "${REPO}/core" "${REPO}/tests" "${REPO}/firmware")

if(NOT EXISTS "${BUNDLE_C}")
    message(FATAL_ERROR "no ${BUNDLE_C}: run tests/device/run.sh (it runs bundle.py first)")
endif()
if(NOT EXISTS "${BID_LIB}")
    message(FATAL_ERROR "no ${BID_LIB}: run TARGET=${target} scripts/fetch-intel-dfp.sh")
endif()

target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${BID_INC}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE AB_DEVICE TEST_STACK=40960
    DECIMAL_CALL_BY_REFERENCE=0 DECIMAL_GLOBAL_ROUNDING=0 DECIMAL_GLOBAL_EXCEPTION_FLAGS=0)
set_source_files_properties("${REPO}/tests/abn_test.c" PROPERTIES COMPILE_DEFINITIONS "main=abn_test_main")
set_source_files_properties("${REPO}/tests/xnum_test.c" PROPERTIES COMPILE_DEFINITIONS "main=xnum_test_main")
target_link_libraries(${COMPONENT_LIB} PRIVATE "${BID_LIB}")
