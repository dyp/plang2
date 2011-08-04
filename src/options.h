/// \file options.h
///

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <string>
#include <vector>

// Pretty print.
enum {
    PP_NONE = 0,
    PP_FLAT = 1,
    PP_AST  = 2,
    PP_LEX  = 4,
};

// Type check.
enum {
    TC_NONE = 0,
    TC_ON   = 1,
};

// Back end.
enum {
    BE_NONE = 0,
    BE_PP   = 1,
    BE_C    = 2,
};

struct Options {
    typedef std::vector<std::string> Args;

    int prettyPrint;
    int typeCheck;
    int backEnd;
    std::string strInputFilename;
    std::string strOutputFilename;
    Args args;
    bool bCheckSemantics;
    bool bVerbose;

    Options();
    static bool init(size_t _cArgs, const char **_pArgs);
    static Options &instance();
};

#endif // OPTIONS_H_

