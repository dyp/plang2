cmake_minimum_required(VERSION 2.8)

add_test(Sample
    ${PROJECT_SOURCE_DIR}/tools/test.sh
    ${PROJECT_SOURCE_DIR}/tools
    ${PROJECT_BINARY_DIR}/src
    ${PROJECT_SOURCE_DIR}/tests/sample.psrc)
