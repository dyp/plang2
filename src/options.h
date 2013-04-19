/// \file options.h
///

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <string>
#include <vector>

// Pretty print.
enum {
    PP_NONE       = 0,
    PP_FLAT       = 1,
    PP_AST        = 2,
    PP_LEX        = 4,
    PP_SYNTAX     = 8,
};

// Type check.
enum {
    TC_NONE = 0,
    TC_ON   = 1,
    TC_SOFT = 2,
};

// Back end.
enum {
    BE_NONE = 0,
    BE_PP   = 1,
    BE_C    = 2,
    BE_PVS  = 4,
};

// Optimizing trasformation (tail-recursion elimination, predicate inlining and variable merging).
enum {
    OT_NONE = 0,
    OT_TRE = 1,
    OT_PI = 2,
    OT_VM = 4,
};

struct Options {
    typedef std::vector<std::string> Args;

    int prettyPrint;
    int typeCheck;
    int backEnd;
    int transformation;
    std::string strInputFilename;
    std::string strOutputFilename;
    Args args;
    bool bOptimize;
    bool bCheckSemantics;
    bool bVerify;
    bool bVerbose;

    Options();
    static bool init(size_t _cArgs, const char **_pArgs);
    static Options &instance();
};

#endif // OPTIONS_H_

