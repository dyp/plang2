cmake_minimum_required(VERSION 2.8)
project(PLang)

enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

find_library(GMPXXLIB gmpxx)

if (NOT GMPXXLIB)
    add_subdirectory(mpir-2.3.0)
    include_directories(${CMAKE_BINARY_DIR}/mpir-2.3.0/include)
    link_directories(${CMAKE_BINARY_DIR}/mpir-2.3.0/lib)
endif()

add_subdirectory(src)
add_subdirectory(tests)

if (NOT GMPXXLIB)
    add_dependencies(plang mpir)
    add_dependencies(verification_tests mpir)
endif()
