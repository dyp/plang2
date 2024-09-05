#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <fstream>
#include <vector>

#include "indenting_stream.h"
#include "utils.h"

static
void _printUsage() {
    std::cerr << "Usage:\n\n"
        << "    test_indenting_stream [OPTIONS] FILE\n\n"
        << "    FILE must end with with newline character.\n\n"
        << "Options\n\n"
        << "        --help                    Show this message\n";
}

typedef std::vector<std::string> Args;

static
bool _handleNotAnOption(const std::string &_val, void *_p) {
    Args &args = *(Args *)_p;
    args.push_back(_val);
    return true;
}

#ifndef WIDE
#define CHAR char
#define STRING std::string
#define COUT std::cout
#define CERR std::cerr
#define IFSTREAM std::ifstream
#define L(X) X
#define STRTOUL(...) strtoul(__VA_ARGS__)
#else
#define CHAR wchar_t
#define STRING std::wstring
#define COUT std::wcout
#define CERR std::wcerr
#define IFSTREAM std::wifstream
#define L(X) L##X
#define STRTOUL(...) wcstoul(__VA_ARGS__)
#endif

int main(int _argc, const char **_argv) {
    bool bHelp = false;
    Args args;

    Option options[] = {
            { "help",   'h',    nullptr,    &bHelp, nullptr,    false },
            { nullptr,    0,    nullptr,   nullptr, nullptr,    false },
    };

    if (!parseOptions(_argc - 1, _argv + 1, options, &args, _handleNotAnOption)) {
        _printUsage();
        exit(EXIT_FAILURE);
    }

    if (bHelp) {
        _printUsage();
        exit(EXIT_SUCCESS);
    }

    if (args.empty()) {
        std::cerr << "No input filename given" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string strInputFilename = args.front();
    IFSTREAM ifs(strInputFilename);

    if (!ifs.good()) {
        std::cerr << "Cannot open input file " << strInputFilename << std::endl;
        exit(EXIT_FAILURE);
    }

    STRING strIn;
    IndentingStream<CHAR> out(COUT);

    while (std::getline(ifs, strIn).good()) {
        STRING strOut;

        strOut.reserve(strIn.size());

        for (size_t c = 0; c < strIn.size();) {
            if (strIn[c] == L('\\') && c + 1 < strIn.size() &&
                    STRING(L("iunvlabpq")).find(strIn[c + 1]) != STRING::npos)
            {
                ++c;

                if (strIn[c] == L('n')) {
                    strOut.push_back(L('\n'));
                    ++c;
                } else {
                    if (strIn[c] == L('i')) {
                        out << strOut << indent;
                        ++c;
                    } else if (strIn[c] == L('u')) {
                        out << strOut << unindent;
                        ++c;
                    } else if (strIn[c] == L('a')) {
                        out << strOut << setInline(true);
                        ++c;
                    } else if (strIn[c] == L('b')) {
                        out << strOut << setInline(false);
                        ++c;
                    } else if (strIn[c] == L('p')) {
                        out << strOut << setVerbatim(true);
                        ++c;
                    } else if (strIn[c] == L('q')) {
                        out << strOut << setVerbatim(false);
                        ++c;
                    } else if (strIn[c] == L('v')) {
                        out << strOut << setIndentation(strIn.substr(c + 1));
                        c = strIn.size();
                    } else if (strIn[c] == L('l')) {
                        errno = 0;

                        CHAR *pEnd = nullptr;
                        const unsigned long uLevel = STRTOUL(
                                strIn.data() + c + 1, &pEnd, 10);

                        if (pEnd == strIn.data() + c + 1) {
                            CERR << L("Error: level should be an integer: ") <<
                                    STRING(pEnd) << std::endl;
                            exit(EXIT_FAILURE);
                        }

                        if ((uLevel == 0 && errno == EINVAL) ||
                                (uLevel == ULONG_MAX && errno == ERANGE))
                        {
                            perror("strtoul()");
                            exit(EXIT_FAILURE);
                        }

                        out << strOut << setLevel(uLevel);
                        c = pEnd - strIn.data();
                    }

                    strOut.clear();
                }

                continue;
            }

            strOut.push_back(strIn[c]);
            ++c;
        }

        out << strOut << std::endl;
    }

    return EXIT_SUCCESS;
}
