#include <ir/statements.h>

namespace ir {

NodePtr Jump::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Label>(getDestination()), _cloner.get<Label>(getLabel()));
}

NodePtr Assignment::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Expression>(getLValue()), _cloner.get<Expression>(getExpression()), _cloner.get<Label>(getLabel()));
}

NodePtr CallBranch::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Statement>(getHandler()));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr Call::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Expression>(getPredicate(), true), _cloner.get<Label>(getLabel()));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    pCopy->getBranches().appendClones(getBranches(), _cloner);
    pCopy->getDeclarations().appendClones(getDeclarations(), _cloner);
    return pCopy;
}

NodePtr Multiassignment::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Label>(getLabel()));
    pCopy->getLValues().appendClones(getLValues(), _cloner);
    pCopy->getExpressions().appendClones(getExpressions(), _cloner);
    return pCopy;
}

NodePtr SwitchCase::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Statement>(getBody()));
    pCopy->getExpressions().appendClones(getExpressions(), _cloner);
    return pCopy;
}

NodePtr Switch::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Expression>(getArg()), _cloner.get<Statement>(getDefault()),
            _cloner.get<VariableDeclaration>(getParamDecl()), _cloner.get<Label>(getLabel()));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr If::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Expression>(getArg()), _cloner.get<Statement>(getBody()),
            _cloner.get<Statement>(getElse()), _cloner.get<Label>(getLabel()));
}

NodePtr For::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<VariableDeclaration>(getIterator()), _cloner.get<Expression>(getInvariant()), _cloner.get<Statement>(getIncrement()),
            _cloner.get<Statement>(getBody()), _cloner.get<Label>(getLabel()));
}

NodePtr While::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Expression>(getInvariant()), _cloner.get<Statement>(getBody()), _cloner.get<Label>(getLabel()));
}

NodePtr Break::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Label>(getLabel()));
}

NodePtr Send::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Process>(getReceiver(), true), _cloner.get<Message>(getMessage()), _cloner.get<Label>(getLabel()));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    return pCopy;
}

NodePtr MessageHandler::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Message>(getMessage()), _cloner.get<Statement>(getBody()));
}

NodePtr Receive::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Expression>(getTimeout()), _cloner.get<Statement>(getTimeoutHandler()), _cloner.get<Label>(getLabel()));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr With::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Statement>(getBody()), _cloner.get<Label>(getLabel()));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    return pCopy;
}
}
