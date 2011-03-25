cmake_minimum_required(VERSION 2.8)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mpirxx.h ${CMAKE_CURRENT_BINARY_DIR}/mpirxx.h)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.h *.cc)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/libgmp.a
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/libgmpxx.a
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${SOURCES}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure --enable-cxx --enable-gmpcompat --enable-static --disable-shared --prefix=${CMAKE_CURRENT_BINARY_DIR}
    COMMAND make all install
)

add_custom_target(mpir DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lib/libgmp.a ${CMAKE_CURRENT_BINARY_DIR}/lib/libgmpxx.a)
