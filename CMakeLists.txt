cmake_minimum_required(VERSION 2.8)
project(PLang)

enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -fpermissive")

find_library(GMPXXLIB gmpxx)

if (NOT GMPXXLIB)
    message(FATAL_ERROR "GMPXXLIB not found.")
endif()

include(FindPkgConfig)

pkg_check_modules(CVC3LIB cvc3)

if (NOT CVC3LIB_FOUND)
    message(FATAL_ERROR "CVC3LIB not found.")
endif()

include_directories(${CVC3LIB_INCLUDE_DIRS})

add_subdirectory(src)
add_subdirectory(tests)
