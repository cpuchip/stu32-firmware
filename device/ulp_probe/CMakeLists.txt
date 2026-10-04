# Does the ULP-RISC-V run through deep sleep with Rev A's configuration? (abacus's Rev A review, R2)
# A probe for the 4.3" board's S3. Run with device/ulp_probe/run.sh PORT.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ulp_probe)
