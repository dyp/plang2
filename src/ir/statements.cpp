#include <ir/statements.h>

namespace ir {

NodePtr Jump::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, Jump(_cloner.get(getDestination()), _cloner.get(getLabel())));
}

NodePtr Assignment::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, Assignment(_cloner.get(getLValue()), _cloner.get(getExpression()), _cloner.get(getLabel())));
}

NodePtr CallBranch::clone(Cloner &_cloner) const {
    CallBranchPtr pCopy = NEW_CLONE(this, _cloner, CallBranch(_cloner.get(getHandler())));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr Call::clone(Cloner &_cloner) const {
    CallPtr pCopy = NEW_CLONE(this, _cloner, Call(_cloner.get(getPredicate(), true), _cloner.get(getLabel())));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    pCopy->getBranches().appendClones(getBranches(), _cloner);
    pCopy->getDeclarations().appendClones(getDeclarations(), _cloner);
    return pCopy;
}

NodePtr Multiassignment::clone(Cloner &_cloner) const {
    MultiassignmentPtr pCopy = NEW_CLONE(this, _cloner, Multiassignment(_cloner.get(getLabel())));
    pCopy->getLValues().appendClones(getLValues(), _cloner);
    pCopy->getExpressions().appendClones(getExpressions(), _cloner);
    return pCopy;
}

NodePtr SwitchCase::clone(Cloner &_cloner) const {
    SwitchCasePtr pCopy = NEW_CLONE(this, _cloner, SwitchCase(_cloner.get(getBody())));
    pCopy->getExpressions().appendClones(getExpressions(), _cloner);
    return pCopy;
}

NodePtr Switch::clone(Cloner &_cloner) const {
    SwitchPtr pCopy = NEW_CLONE(this, _cloner, Switch(_cloner.get(getArg()), _cloner.get(getDefault()),
            _cloner.get(getParamDecl()), _cloner.get(getLabel())));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr If::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, If(_cloner.get(getArg()), _cloner.get(getBody()),
            _cloner.get(getElse()), _cloner.get(getLabel())));
}

NodePtr For::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, For(_cloner.get(getIterator()), _cloner.get(getInvariant()), _cloner.get(getIncrement()),
            _cloner.get(getBody()), _cloner.get(getLabel())));
}

NodePtr While::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, While(_cloner.get(getInvariant()), _cloner.get(getBody()), _cloner.get(getLabel())));
}

NodePtr Break::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, Break(_cloner.get(getLabel())));
}

NodePtr Send::clone(Cloner &_cloner) const {
    SendPtr pCopy = NEW_CLONE(this, _cloner, Send(_cloner.get(getReceiver(), true), _cloner.get(getMessage()), _cloner.get(getLabel())));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    return pCopy;
}

NodePtr MessageHandler::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, MessageHandler(_cloner.get(getMessage()), _cloner.get(getBody())));
}

NodePtr Receive::clone(Cloner &_cloner) const {
    ReceivePtr pCopy = NEW_CLONE(this, _cloner, Receive(_cloner.get(getTimeout()), _cloner.get(getTimeoutHandler()), _cloner.get(getLabel())));
    pCopy->appendClones(*this, _cloner);
    return pCopy;
}

NodePtr With::clone(Cloner &_cloner) const {
    WithPtr pCopy = NEW_CLONE(this, _cloner, With(_cloner.get(getBody()), _cloner.get(getLabel())));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    return pCopy;
}
}
