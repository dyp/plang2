cmake_minimum_required(VERSION 2.8)

set(TARGET plang)

set(SOURCES
    autoptr.cpp
    utils.cpp
    ir/base.cpp
    ir/builtins.cpp
    ir/expressions.cpp
    ir/type_derived.cpp
    ir/type_predicate.cpp
    ir/type_array.cpp
    ir/type_subtype.cpp
    ir/types.cpp
    ir/type_struct.cpp
    ir/type_union.cpp
    ir/visitor.cpp
    ir/declarations.cpp
    transformations/tail_recursion_elimination.cpp
    transformations/predicate_inlining.cpp
    backend_c.cpp
    backend_debug.cpp
    backend_pvs.cpp
    collect_constraints.cpp
    static_typecheck.cpp
    lexer.cpp
    llir.cpp
    llir_process.cpp
    numbers.cpp
    options.cpp
    parser_context.cpp
    parser.cpp
    plang.cpp
    optimization.cpp
    pp_flat_tree.cpp
    pp_syntax.cpp
    prettyprinter.cpp
    solve_constraints.cpp
    typecheck.cpp
    generate_semantics.cpp
    generate_name.cpp
    type_lattice.cpp
    term_rewriting.cpp
    node_analysis.cpp
    statement_tree.cpp
    verification_inference.cpp
    verification_context.cpp
    pp_verification.cpp
    name_reset.cpp
    predicate_ordering.cpp
    callgraph.cpp
    generate_callgraph.cpp
    typecheck/compact.cpp
    typecheck/eval.cpp
    typecheck/expand.cpp
    typecheck/explode.cpp
    typecheck/guess.cpp
    typecheck/infer.cpp
    typecheck/lift.cpp
    typecheck/operations.cpp
    typecheck/prune.cpp
    typecheck/refute.cpp
    typecheck/unify.cpp
    sort_module.cpp
)

if (CVC3 AND CVC3LIB_FOUND)
    set(SOURCES ${SOURCES} cvc3_solver.cpp)
endif()

add_definitions(
    -fmessage-length=0 -Wall -Werror -Wno-deprecated
)

add_executable(${TARGET} ${SOURCES})

if (CVC3 AND CVC3LIB_FOUND)
    set_target_properties(${TARGET} PROPERTIES COMPILE_DEFINITIONS "USE_CVC3")
    target_link_libraries(${TARGET} ${CVC3LIB_LIBRARIES})
endif()

include_directories (
    ../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET}
    gmpxx
    gmp
)

install(TARGETS plang
  RUNTIME DESTINATION bin)

