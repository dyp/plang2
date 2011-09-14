cmake_minimum_required(VERSION 2.8)

macro(run_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/src
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

set(TESTS_PARSING
    parsing/logic_mul.psrc
    parsing/logic_gcd.psrc
    parsing/formula_result.psrc
    parsing/field_expr.psrc
    parsing/submodules.psrc
    parsing/subformula_args.psrc
    parsing/quantifier.psrc
)

set(TESTS_INFERENCE
    sample.psrc
    inference/literals.psrc
    inference/literals_structured.psrc
    inference/literals_negative.psrc
)

run_tests("-t0" "${TESTS_PARSING}")
run_tests("-t1" "${TESTS_INFERENCE}")
