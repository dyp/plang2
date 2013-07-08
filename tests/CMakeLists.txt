cmake_minimum_required(VERSION 2.8)

# Tests for statements modification

set(TARGET_ST verification_tests)

set(SOURCES_ST
    ../src/autoptr.cpp
    ../src/utils.cpp
    ../src/ir/base.cpp
    ../src/ir/builtins.cpp
    ../src/ir/expressions.cpp
    ../src/ir/type_derived.cpp
    ../src/ir/type_predicate.cpp
    ../src/ir/type_array.cpp
    ../src/ir/type_subtype.cpp
    ../src/ir/types.cpp
    ../src/ir/type_struct.cpp
    ../src/ir/type_union.cpp
    ../src/ir/visitor.cpp
    ../src/lexer.cpp
    ../src/numbers.cpp
    ../src/options.cpp
    ../src/parser_context.cpp
    ../src/parser.cpp
    ../src/verification_tests.cpp
    ../src/pp_flat_tree.cpp
    ../src/pp_syntax.cpp
    ../src/prettyprinter.cpp
    ../src/statement_tree.cpp
    ../src/node_analysis.cpp
    ../src/term_rewriting.cpp
    ../src/typecheck.cpp
    ../src/solve_constraints.cpp
    ../src/collect_constraints.cpp
    ../src/type_lattice.cpp
    ../src/generate_semantics.cpp
    ../src/generate_name.cpp
    ../src/verification_context.cpp
    ../src/cvc3_solver.cpp
)

add_executable(${TARGET_ST} ${SOURCES_ST})

target_link_libraries(${TARGET_ST} ${CVC3LIB_LIBRARIES})

include_directories (
    ../include
)

target_link_libraries(${TARGET_ST}
    gmpxx
    gmp
)

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

macro(run_st_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_st.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/tests
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

macro(run_cond_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_cond.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/tests
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

macro(run_vf_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_vf.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/src
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

macro(run_pvs_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_pvs.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/src
            ${PROJECT_SOURCE_DIR}/tests/${TEST}
            ${FLAGS})
    endforeach()
endmacro()

macro(run_cvc3_tests FLAGS TESTS)
    foreach(TEST ${TESTS})
        add_test(${TEST}
            ${PROJECT_SOURCE_DIR}/tools/test_cvc3.sh
            ${PROJECT_SOURCE_DIR}/tools
            ${PROJECT_BINARY_DIR}/tests
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
    parsing/literal.psrc
    parsing/lemma_status.psrc
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

set(TESTS_STATEMENT_TREE
    statement_tree/example.psrc
)

set(TESTS_CONDITIONS
    semantics/example.psrc
)

set(TESTS_VERIFICATION
    verification/if_call.psrc
    verification/logic.psrc
    verification/logic_call.psrc
    verification/split.psrc
    verification/submodules.psrc
    verification/switch.psrc
)

set(TESTS_PVS
    backend_pvs/example.psrc
)

set(TESTS_CVC3
    cvc3/literals.psrc
    cvc3/types.psrc
    cvc3/expressions.psrc
    cvc3/replacement.psrc
)

run_tests("-t0" "${TESTS_PARSING}")
run_tests("-t2" "${TESTS_INFERENCE}")
run_tests("-t1" "${TESTS_SEMANTICS}")
run_pp_tests("-t0" "${TESTS_PRETTYPRINTER}")
run_cg_tests("-t2" "${TESTS_CALLGRAPH}")
run_ot_tests("-t2" "${TESTS_TRASFORMATION}")
run_st_tests("-t0" "${TESTS_STATEMENT_TREE}")
run_cond_tests("" "${TESTS_CONDITIONS}")
run_vf_tests("-t1" "${TESTS_VERIFICATION}")
run_pvs_tests("-t1" "${TESTS_PVS}")
run_cvc3_tests("" "${TESTS_CVC3}")
