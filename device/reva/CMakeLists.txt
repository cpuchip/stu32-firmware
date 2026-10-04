# Rev A's firmware (unit 018): the calculator on its own board. Built in the pinned image with
# device/reva/build.sh; not yet run on a board (Rev A is being ordered).
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(abacus_reva)
