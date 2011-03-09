cmake_minimum_required(VERSION 2.8)

set(TESTS
    sample.psrc
    inference/literals.psrc
    inference/literals_structured.psrc
)

foreach(TEST ${TESTS})
    add_test(${TEST}
        ${PROJECT_SOURCE_DIR}/tools/test.sh
        ${PROJECT_SOURCE_DIR}/tools
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/tests/${TEST})
endforeach()
