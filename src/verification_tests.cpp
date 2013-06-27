#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>

#include "ir/statements.h"
#include "lexer.h"
#include "parser.h"
#include "parser_context.h"
#include "pp_syntax.h"
#include "test_statement_tree.h"
#include "test_preconditions.h"
#include "test_cvc3_solver.h"
#include "prettyprinter.h"
#include "options.h"
#include "utils.h"

using namespace lexer;

bool
    bStatementTree = false,
    bPreconditions = false,
    bCVC3 = false;

std::string strFile;

static bool _handleNotAnOption(const std::string &_strVal, void *_p) {
    strFile = _strVal;
    return true;
}

static bool _parseTestOptions(size_t _cArgs, const char **_pArgs) {
    Option options[] = {
        { "statement-tree", 's', NULL, &bStatementTree, NULL, false },
        { "preconditions",  'p', NULL, &bPreconditions, NULL, false },
        { "cvc3",           'c', NULL, &bCVC3,          NULL, false }
    };

    if (!parseOptions(_cArgs, _pArgs, options, NULL, &_handleNotAnOption))
        return false;

    return strFile.empty() ? false : true;
}

int main(int _argc, const char ** _argv) {
    std::locale::global(std::locale(""));

    if (!_parseTestOptions(_argc - 1, _argv + 1))
        return EXIT_FAILURE;

    // Options
    Options::instance().strInputFilename = strFile;

    std::ifstream ifs(strFile.c_str());
    Tokens tokens;

    try {
        tokenize(tokens, ifs);
    } catch (ELexerException & e) {
        std::cerr << strFile << ":" << e.getLine() << ":" << e.getCol()
            << ": error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (bPreconditions || bCVC3)
        Options::instance().typeCheck = TC_ON;
    else
        Options::instance().typeCheck = TC_NONE;

    ir::ModulePtr pModule = parse(tokens);

    if (!pModule)
        return EXIT_FAILURE;

    if (bStatementTree)
        TreePrinter(std::wcout).print(pModule);

    if (bPreconditions)
        PreconditionsPrinter(std::wcout).traverseNode(*pModule);
    else if (bCVC3)
        Cvc3Printer().traverseNode(*pModule);

    return EXIT_SUCCESS;
}
