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

macro(run_cg_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_cg.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/src
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

macro(run_ot_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_ot.sh
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
    parsing/replacement.psrc
    parsing/variable_declarations.psrc
)

set(TESTS_INFERENCE
    sample.psrc
    inference/literals.psrc
    inference/literals_structured.psrc
    inference/literals_negative.psrc
    inference/param_array.psrc
    inference/procedure_call.psrc
    inference/binder.psrc
    inference/lambda.psrc
    inference/struct_constructor.psrc
    inference/array_part.psrc
    inference/array_constructor.psrc
    inference/array_iterator.psrc
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
    prettyprinter/array_constructor.psrc
    prettyprinter/array_iterator.psrc
    prettyprinter/binder.psrc
    prettyprinter/cast.psrc
    prettyprinter/field_expr.psrc
    prettyprinter/map_constructor.psrc
    prettyprinter/predicate_type.psrc
    prettyprinter/struct_constructor.psrc
    prettyprinter/types.psrc
    prettyprinter/union_constructor.psrc
    prettyprinter/union_element.psrc
    prettyprinter/decls_order.psrc
)

set(TESTS_CALLGRAPH
    callgraph/base_test.psrc
    callgraph/multiple_calls.psrc
    callgraph/lambda.psrc
    callgraph/functioncall.psrc
    callgraph/call_results.psrc
    callgraph/call_arguments.psrc
    callgraph/same_names.psrc
)

set(TESTS_SEMANTICS
    semantics/sample1.psrc
    semantics/sample2.psrc
    semantics/switch_real.psrc
    semantics/arCons.psrc
    semantics/arMod.psrc
    semantics/compat.psrc
    semantics/arSum.psrc
)

set(TESTS_TRASFORMATION
    transformation/tail_recursion.psrc
)

run_tests("-t0" "${TESTS_PARSING}")
run_tests("-t2" "${TESTS_INFERENCE}")
run_tests("-t1" "${TESTS_SEMANTICS}")
run_pp_tests("-t0" "${TESTS_PRETTYPRINTER}")
run_cg_tests("-t2" "${TESTS_CALLGRAPH}")
run_ot_tests("-t2" "${TESTS_TRASFORMATION}")
