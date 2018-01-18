//============================================================================
// Name        : plang.cpp
// Author      : Nikita Karnauhov (jinx@ac-sw.com)
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>

#include "ir/statements.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "prettyprinter.h"
#include "llir.h"
#include "backend_debug.h"
#include "backend_c.h"
#include "backend_pvs.h"
#include "typecheck.h"
#include "parser_context.h"
#include "optimization.h"
#include "pp_flat_tree.h"
#include "pp_syntax.h"
#include "options.h"
#include "generate_semantics.h"
#include "verification.h"
#include "generate_callgraph.h"
#include "predicate_ordering.h"
#include "transformations/tail_recursion_elimination.h"
#include "transformations/predicate_inlining.h"

#ifdef USE_CVC3
#include "cvc3_solver.h"
#endif

#include "name_reset.h"
#include "check_assignments.h"
#include "type_lattice.h"
#include "term_rewriting.h"

using namespace lexer;

int main(int _argc, const char ** _argv) {
    std::locale::global(std::locale(std::locale(""), "C", std::locale::numeric));

    if (!Options::init(_argc - 1, _argv + 1))
        return EXIT_FAILURE;

    const std::string &strFile = Options::instance().strInputFilename;
    std::ifstream ifs(strFile.c_str());
    Tokens tokens;

    try {
        tokenize(tokens, ifs);
    } catch (ELexerException & e) {
        std::cerr << strFile << ":" << e.getLine() << ":" << e.getCol()
            << ": error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (Options::instance().prettyPrint & PP_LEX) {
        for (Tokens::const_iterator i = tokens.begin(); i != tokens.end(); ++ i) {
            const Token & tok = * i;
            std::wcout << fmtInt(tok.getLine(), L"%5d")
                    << ":" << fmtInt(tok.getCol(), L"%3d")
                    << " (" << fmtInt(tok.getKind(), L"%3d") << ")"
                    << " \"" << tok.getValue() << "\"" << std::endl;
        }
    }

    if (Options::instance().bSolveTypes) {
        tc::Formulas formulas;
        bool bResult = false;
        FreshTypeNames names;

        if (tc::ContextPtr pCtx = parseTypeConstraints(tokens, formulas, names)) {
            bResult = tc::solve(pCtx);
            PrettyPrinterBase::setFreshTypeNames(names);
            prettyPrint(*pCtx, std::wcout);
        }

        std::wcout << (bResult ? L"Success" : L"Failed") << std::endl;
        return bResult ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (ir::ModulePtr pModule = parse(tokens)) {
        if (!Options::instance().bKeepNames)
            resetNames(*pModule);

        try {
            ir::CheckAssignments().traverseNode(*pModule);
        } catch (std::runtime_error &e) {
            std::cerr << strFile << ": " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        if (Options::instance().bCheckSemantics)
            pModule = processPreConditions(*pModule);

        if (Options::instance().verify != V_NONE)
            pModule = vf::verify(pModule);


#ifdef USE_CVC3
        if (Options::instance().bCheckValidity)
            cvc3::checkValidity(pModule);
#endif

        if (Options::instance().transformation & OT_TRE)
            tailRecursionElimination(*pModule);

        if (Options::instance().transformation & OT_PI)
            predicateInlining(*pModule);

        if (Options::instance().bMoveOut) {
            tr::moveOutExpressions(pModule);
            tr::moveOutStructuredTypes(pModule);
        }

        if (Options::instance().bOptimize)
            optimize(*pModule);

        if (Options::instance().prettyPrint & PP_FLAT)
            prettyPrintFlatTree(*pModule);

        if (Options::instance().prettyPrint & PP_AST)
            prettyPrint(*pModule, std::wcout);

        if (Options::instance().prettyPrint & PP_SYNTAX)
            pp::prettyPrintSyntax(*pModule, std::wcout, NULL, true);

        if (Options::instance().prettyPrint & PP_CALLGRAPH)
            printModuleSCCCallGraph(*pModule, std::wcout);

        if (Options::instance().backEnd == BE_NONE)
            return EXIT_SUCCESS;

        if (Options::instance().backEnd == BE_PVS) {
            std::string strOut = Options::instance().strOutputFilename;
            if (!strOut.empty()) {
                std::wofstream ofs(strOut.c_str());
                generatePvs(*pModule, ofs);
            }
            else
                generatePvs(*pModule);
            return EXIT_SUCCESS;
        }

        llir::Module module;

        llir::translate(module, * pModule);

        if (Options::instance().backEnd & BE_PP)
            backend::generateDebug(module, std::wcout);

        if (Options::instance().backEnd & BE_C) {
            std::string strOut = Options::instance().strOutputFilename;
            std::wofstream ofs(strOut.empty() ? (strFile + ".c").c_str() : strOut.c_str());
            backend::generateC(module, ofs);
        }

        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
