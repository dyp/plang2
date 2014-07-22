/// \file solve_constraints.cpp
///

#include <iostream>
#include <vector>

#include "typecheck.h"
#include "prettyprinter.h"
#include "options.h"
#include "typecheck/operations.h"

using namespace ir;

typedef std::set<tc::FreshTypePtr, PtrLess<tc::FreshType> > FreshTypeSet;
typedef tc::ContextStack CS;

class Solver {
public:
    bool run();
    bool sequence(int &_result);
    bool fork();
    tc::Context &context() { return *CS::top(); }
};

bool Solver::fork() {
    tc::Formulas::iterator iCF = context()->beginCompound();

    if (iCF == context()->end())
        return false;

    tc::CompoundFormulaPtr pCF = iCF->as<tc::CompoundFormula>();

    context()->erase(iCF);

    tc::ContextPtr pCopy = clone(context());

    context().insertFormulas(pCF->getPart(0));
    pCF->getPart(0).pFlags->mergeTo(context().flags());

    if (pCF->size() > 2) {
        pCF->removePart(0);
        (*pCopy)->insert(pCF);
    } else {
        pCopy->insertFormulas(pCF->getPart(1));
        pCF->getPart(1).pFlags->mergeTo(pCopy->flags());
    }

    CS::push(pCopy);

    return true;
}

bool Solver::sequence(int &_result) {
    bool bModified = false;
    bool bIterationModified;
    size_t cStep = 0;
    std::vector<Auto<tc::Operation> > ops{
        tc::Operation::unify(),
        tc::Operation::lift(),
        tc::Operation::prune(),
        tc::Operation::compact(),
        tc::Operation::eval(),
        tc::Operation::expand(),
        tc::Operation::refute(),
        tc::Operation::infer(),
        tc::Operation::explode(),
        tc::Operation::guess()
    };

    _result = tc::Formula::UNKNOWN;

    do {
        bIterationModified = false;

        for (const Auto<tc::Operation> & pOperation : ops) {
            if (bIterationModified && pOperation->getRestartIteration())
                break;

            if (pOperation->run(_result)) {
                if (Options::instance().bVerbose) {
                    std::wcout << std::endl << pOperation->getName() << L" [" << cStep << L"]:" << std::endl;
                    prettyPrint(context(), std::wcout);
                }

                bIterationModified = true;

                if (_result == tc::Formula::FALSE)
                    break;
            }
        }

        bModified |= bIterationModified;
        ++cStep;
    } while (bIterationModified && _result != tc::Formula::FALSE);

    return bModified;
}

bool Solver::run() {
    int result = tc::Formula::UNKNOWN;
    std::list<tc::ContextPtr> processed;

    do {
        const bool bChanged = sequence(result);

        if (!bChanged && result != tc::Formula::FALSE && fork()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Fork:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            continue;
        }

        if (bChanged && result != tc::Formula::FALSE)
            continue;

        if (result != tc::Formula::FALSE)
            processed.push_back(CS::top());

        if (Options::instance().bVerbose)
            std::wcout << std::endl << (result == tc::Formula::FALSE ? L"Refuted" : L"Affirmed") << std::endl;

        CS::pop();
    } while (!CS::empty());

    if (processed.empty())
        result = tc::Formula::FALSE;
    else {
        if (::next(processed.begin()) == processed.end())
            CS::push(processed.front());
        else {
            // Recombine all results into a compound formula and simplify it.
            tc::CompoundFormulaPtr pCF = new tc::CompoundFormula();

            for (std::list<tc::ContextPtr>::iterator i = processed.begin(); i != processed.end(); ++i) {
                tc::Context &ctx = **i;
                tc::Formulas &part = pCF->addPart();

                CS::push(*i);
                part.insertFormulas(*ctx.pFormulas);

                for (tc::Formulas::iterator j = ctx.pSubsts->begin(); j != ctx.pSubsts->end(); ++j) {
                    tc::FormulaPtr pSubst = *j;

                    if (pSubst->getLhs().as<tc::FreshType>()->getFlags() == tc::FreshType::PARAM_IN)
                        part.insert(new tc::Formula(tc::Formula::SUBTYPE, pSubst->getLhs(), pSubst->getRhs()));
                    else if (pSubst->getLhs().as<tc::FreshType>()->getFlags() == tc::FreshType::PARAM_OUT)
                        part.insert(new tc::Formula(tc::Formula::SUBTYPE, pSubst->getRhs(), pSubst->getLhs()));
                    else
                        part.insert(pSubst);
                }

                part.pFlags = ctx.pFormulas->pFlags;
                CS::pop();
            }

            CS::push(ptr(new tc::Context()));
            context()->insert(pCF);

            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Recombine:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            sequence(result);
            assert(result != tc::Formula::FALSE);
        }

        result = context()->empty() ? tc::Formula::TRUE : tc::Formula::UNKNOWN;

        if (Options::instance().bVerbose) {
            std::wcout << std::endl << L"Solution:" << std::endl;
            prettyPrint(context(), std::wcout);
        }
    }

    if (Options::instance().bVerbose) {
        switch (result) {
            case tc::Formula::UNKNOWN:
                std::wcout << std::endl << L"Inference incomplete" << std::endl;
                break;
            case tc::Formula::TRUE:
                std::wcout << std::endl << L"Inference successful" << std::endl;
                break;
            case tc::Formula::FALSE:
                std::wcout << std::endl << L"Type error" << std::endl;
                break;
        }
    }

    return result != tc::Formula::FALSE;
}

bool tc::solve(const tc::ContextPtr &_pContext) {
    CS::clear();
    CS::push(_pContext);

    if (Options::instance().bVerbose) {
        std::wcout << std::endl << L"Solving:" << std::endl;
        prettyPrint(*CS::top(), std::wcout);
    }

    const bool bResult = Solver().run();

    CS::pop();

    return bResult;
}
