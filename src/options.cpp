/// \file options.cpp
///

#include <string.h>
#include <iostream>

#include "options.h"
#include "utils.h"

static
bool _handlePrettyPrint(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;

    if (_val == "none" || _val == "0")
        opts.prettyPrint = PP_NONE;
    else if (_val == "flat" || _val == "1")
        opts.prettyPrint |= PP_FLAT;
    else if (_val == "ast" || _val == "2")
        opts.prettyPrint |= PP_AST;
    else if (_val == "lex" || _val == "3")
        opts.prettyPrint |= PP_LEX;
    else if (_val == "p" || _val == "4")
        opts.prettyPrint |= PP_SYNTAX;
    else
        return false;

    return true;
}

static
bool _handleBackEnd(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;

    if (_val == "none" || _val == "0")
        opts.backEnd = BE_NONE;
    else if (_val == "pp" || _val == "1")
        opts.backEnd |= BE_PP;
    else if (_val == "c" || _val == "2")
        opts.backEnd |= BE_C;
    else if (_val == "pvs" || _val == "3")
        opts.backEnd |= BE_PVS;
    else
        return false;

    return true;
}

static
bool _handleTypeCheck(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;

    if (_val == "none" || _val == "0")
        opts.typeCheck = TC_NONE;
    else if (_val == "on" || _val == "1")
        opts.typeCheck = TC_ON;
    else if (_val == "soft" || _val == "2")
        opts.typeCheck = TC_SOFT;
    else
        return false;

    return true;
}

static
bool _handleTransformation(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;

    if (_val == "none" || _val == "0")
        opts.transformation = OT_NONE;
    else if (_val == "tailrec" || _val == "1")
        opts.transformation = OT_TRE;
    else if (_val == "predinline" || _val == "2")
        opts.transformation = OT_PI;
    else if (_val == "varmerge" || _val == "3")
        opts.transformation = OT_VM;
    else
        return false;

    return true;
}

static
bool _handleVerify(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;

    if (_val == "none" || _val == "0")
        opts.verify = V_NONE;
    else if (_val == "nothing" || _val == "1")
        opts.verify = V_NOTHING;
    else if (_val == "formulas" || _val == "2")
        opts.verify = V_FORMULAS;
    else if (_val == "verbose" || _val == "3")
        opts.verify |= V_VERBOSE;
    else
        return false;

    return true;
}

static
bool _handleNotAnOption(const std::string &_val, void *_p) {
    Options &opts = *(Options *)_p;
    opts.args.push_back(_val);
    return true;
}

static
void _printUsage() {
    std::cerr << "Usage:\n\n"
        << "    plang [OPTIONS] FILE\n\n"
        << "Options\n\n"
        << "    -p, --prettyprint=TYPE        Pretty-printer mode, where TYPE is 'none' (0), 'flat' (1), 'ast' (2), 'lex' (3), 'p' (4)\n"
        << "    -b, --backend=TYPE            Use backend, where TYPE is 'none' (0), 'pp' (1), 'c' (2), 'pvs' (3)\n"
        << "    -t, --typecheck=TYPE          Do typecheck, where TYPE is 'none' (0), 'on' (1), 'soft' (2)\n"
        << "    -T, --transformation=TYPE     Do optimizing transformation, where TYPE is 'none' (0), 'tailrec' (1), 'predinline' (2), 'varmerge' (3)\n"
        << "    -e, --verify=TYPE             Generate logical conditions for proving program correctness,\n"
        << "                                  where TYPE is 'none' (0), 'nothing' (1), 'formulas' (2), 'verbose' (3)\n"
        << "    -o, --output=FILE             Output file name\n"
        << "    -v, --verbose                 Print debug info\n"
        << "    -O, --optimize                Optimize logical expressions\n"
        << "    -s, --check-semantics         Generate logical conditions for proving semantic correctness\n"
        << "    -a, --check-validity          Check validity of declared lemmas\n"
        << "        --help                    Show this message\n";
}

bool Options::init(size_t _cArgs, const char **_pArgs) {
    bool bHelp = false;
    Option options[] = {
        { "prettyprint",     'p', _handlePrettyPrint, NULL,                        NULL,                          true  },
        { "backend",         'b', _handleBackEnd,     NULL,                        NULL,                          true  },
        { "typecheck",       't', _handleTypeCheck,   NULL,                        NULL,                          false },
        { "transformation",  'T', _handleTransformation,NULL,                      NULL,                          true  },
        { "verify",          'e', _handleVerify,      NULL,                        NULL,                          false },
        { "output",          'o', NULL,               NULL,                        &instance().strOutputFilename, false },
        { "verbose",         'v', NULL,               &instance().bVerbose,        NULL,                          false },
        { "help",            'h', NULL,               &bHelp,                      NULL,                          false },
        { "optimize",        'O', NULL,               &instance().bOptimize,       NULL,                          false },
        { "check-semantics", 's', NULL,               &instance().bCheckSemantics, NULL,                          false },
        { "check-validity",  'a', NULL,               &instance().bCheckValidity,  NULL,                          false },
        { NULL,               0,  NULL,               NULL,                        NULL,                          false }
    };

    if (parseOptions(_cArgs, _pArgs, options, &instance(), _handleNotAnOption)) {
        if (instance().verify != V_NONE && instance().bCheckSemantics) {
            std::cerr << "--check-semantics and --verify are mutually exclusive" << std::endl;
            return false;
        }

        if (!instance().args.empty()) {
            instance().strInputFilename = instance().args.front();
            return true;
        }

        std::cerr << "No input filename given" << std::endl;
    }

    _printUsage();
    return false;
}

Options::Options() :
    prettyPrint(PP_NONE),
    typeCheck(TC_ON),
    backEnd(BE_NONE),
    transformation(OT_NONE),
    verify(V_NONE),
    bOptimize(false),
    bCheckSemantics(false),
    bCheckValidity(false),
    bVerbose(false)
{
}

Options &Options::instance() {
    static Options g_options;
    return g_options;
}
