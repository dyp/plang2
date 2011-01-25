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

using namespace lexer;

int main(int _argc, const char ** _argv) {
    if (_argc < 2)
        return EXIT_FAILURE;

    const std::string strFile(_argv[1]);
    std::ifstream ifs(strFile.c_str());
    Tokens tokens;

    try {
        tokenize(tokens, ifs);
    } catch (ELexerException & e) {
        std::cerr << strFile << ":" << e.getLine() << ":" << e.getCol()
            << ": error: " << e.what() << std::endl;
    }

# if 1
    for (Tokens::const_iterator i = tokens.begin(); i != tokens.end(); ++ i) {
        const Token & tok = * i;
        std::wcout << strFile.c_str() << ":" << tok.getLine() << ":" << tok.getCol()
            << ": token \"" << tok.getValue() << "\" (" << tok.getKind() << ")" << std::endl;
    }
# endif

    ir::Module * pModule;

    if (parse(tokens, pModule)) {
        std::wcout << L"module:\n";
        prettyPrint(* pModule, std::wcout);
        std::wcout << std::endl;
        return 0;
    }

    if (pModule) {
        llir::Module module;

        llir::translate(module, * pModule);

        backend::generateDebug(module, std::wcout);

        std::wofstream ofs((strFile + ".c").c_str());

        backend::generateC(module, ofs);

        delete pModule;
    }

	return 0;
}
