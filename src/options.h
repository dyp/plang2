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
    PP_CALLGRAPH  = 16,
};

// Type check.
enum {
    TC_NONE       = 0,
    TC_FULL       = 1,
    TC_SOFT       = 2,
    TC_PREPROCESS = 3,
};

// Back end.
enum {
    BE_NONE = 0,
    BE_PP   = 1,
    BE_C    = 2,
    BE_PVS  = 4,
};

// Optimizing transformation (tail-recursion elimination, predicate inlining and variable merging).
enum {
    OT_NONE = 0,
    OT_TRE = 1,
    OT_PI = 2,
    OT_VM = 4,
};

// Verify.
enum {
    V_NONE      = 0,
    V_NOTHING   = 1,
    V_FORMULAS  = 2,
    V_VERBOSE   = 3,
};

struct Options {
    typedef std::vector<std::string> Args;

    int prettyPrint;
    int typeCheck;
    int backEnd;
    int transformation;
    int verify;
    std::string strInputFilename;
    std::string strOutputFilename;
    Args args;
    bool bOptimize;
    bool bCheckSemantics;
    bool bCheckValidity;
    bool bVerbose;

    Options();
    static bool init(size_t _cArgs, const char **_pArgs);
    static Options &instance();
};

#endif // OPTIONS_H_

