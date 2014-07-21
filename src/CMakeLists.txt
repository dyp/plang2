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
    cvc3_solver.cpp
    name_reset.cpp
    predicate_ordering.cpp
    callgraph.cpp
    generate_callgraph.cpp
    typecheck/guess.cpp
    typecheck/expand.cpp
    typecheck/infer.cpp
    typecheck/operations.cpp
)

add_definitions(
    -fmessage-length=0 -Wall -Werror
)

add_executable(${TARGET} ${SOURCES})

target_link_libraries(${TARGET} ${CVC3LIB_LIBRARIES})

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

