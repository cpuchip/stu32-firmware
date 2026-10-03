# The on-chip test app (ESP-IDF v5.4). Build through tests/device/run.sh, which generates the
# vector bundle, cross-builds the decimal library and runs idf.py in Espressif's pinned image.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(abacus_device_test)
