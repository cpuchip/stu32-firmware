# The 4.3" S3 stand-in for Rev A (unit 018): the calculator with the LS027's frame and a drawn
# keyboard on the board's RGB screen and touch panel. Build and flash with device/standin/run.sh.
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(abacus_standin)
