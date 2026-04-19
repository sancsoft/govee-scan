# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(CONFIG_PARTITION_TABLE_CUSTOM TRUE)
set(CONFIG_PARTITION_TABLE_CUSTOM_FILENAME "partitions.csv")
set(CONFIG_PARTITION_TABLE_OFFSET 0x10000)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(govee-scan)
