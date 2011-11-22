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
#include "typecheck.h"
#include "parser_context.h"
#include "reduce_expression.h"
#include "pp_flat_tree.h"
#include "pp_syntax.h"
#include "options.h"
#include "generate_semantics.h"

using namespace lexer;

int main(int _argc, const char ** _argv) {
    std::locale::global(std::locale(""));

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

    if (ir::ModulePtr pModule = parse(tokens)) {
        if (Options::instance().bCheckSemantics)
            pModule = processPreConditions(*pModule);

        if (Options::instance().bOptimize)
            reduceExpression(*pModule);

        if (Options::instance().prettyPrint & PP_FLAT)
            prettyPrintFlatTree(*pModule);

        if (Options::instance().prettyPrint & PP_AST)
            prettyPrint(*pModule, std::wcout);

        if (Options::instance().prettyPrint & PP_SYNTAX)
            prettyPrintSyntax(*pModule, std::wcout);

        if (Options::instance().backEnd == BE_NONE)
            return EXIT_SUCCESS;

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
