/// \file test_cvc3_solver.h
///

#ifndef CVC3_SOLVER_H_
#define CVC3_SOLVER_H_

#include <iostream>
#include <sstream>
#include <string>

#include "ir/visitor.h"
#include "utils.h"
#include "cvc3_solver.h"
#include "cvc3/queryresult.h"

class Cvc3Printer : public ir::Visitor {
public:
    Cvc3Printer(std::wostream &_os = std::wcout) : m_os(_os) {}

    bool visitVariableDeclaration(const ir::VariableDeclarationPtr& _var) override {
        if (_var->getValue()) {
            std::stringstream ss;
            cvc3::printImage(*_var->getValue(), ss);
            m_os << strWiden(ss.str());
        }
        return true;
    }
    bool visitLemmaDeclaration(const ir::LemmaDeclarationPtr& _lemma) override {
        std::stringstream ss;
        cvc3::printImage(*_lemma.getProposition(), ss);
        m_os << strWiden(ss.str()) <<
            cvc3::fmtResult(cvc3::checkValidity(_lemma.getProposition())) << "\n";
        return true;
    }
    bool visitTypeDeclaration(const ir::TypeDeclarationPtr& _type) override {
        std::stringstream ss;
        cvc3::printImage(*_type.getType(), ss);
        m_os << strWiden(ss.str());
        return true;
    }

private:
    std::wostream &m_os;
};

#endif // CVC3_SOLVER_H_
