# A test driver for strpp on FreeRTOS: see ReadMe.md. strpp is the component two
# directories up, whatever that checkout is called.
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(freertos_console)
