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

macro(run_pp_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_pp.sh
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
    parsing/array_subtype.psrc
    parsing/quantifier.psrc
)

set(TESTS_INFERENCE
    sample.psrc
    inference/literals.psrc
    inference/literals_structured.psrc
    inference/literals_negative.psrc
    inference/param_array.psrc
)

set(TESTS_PRETTYPRINTER
    prettyprinter/simply_formulas.psrc
    prettyprinter/call_branch_label_after_branch_arg.psrc
    prettyprinter/hyperfunction_call.psrc
    prettyprinter/jumps_and_labels.psrc
    prettyprinter/labels_and_simply_operators.psrc
    prettyprinter/modules_and_pred_in_pred.psrc
    prettyprinter/type_and_variable_decls.psrc
    prettyprinter/var_decl_in_call.psrc
)

run_tests("-t0" "${TESTS_PARSING}")
run_tests("-t2" "${TESTS_INFERENCE}")
run_pp_tests("-t0" "${TESTS_PRETTYPRINTER}")
