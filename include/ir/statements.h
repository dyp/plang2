/// \file statements.h
/// Internal structures representing statements.
///


#ifndef STATEMENTS_H_
#define STATEMENTS_H_

#include "base.h"
#include "expressions.h"
#include "types.h"
#include "declarations.h"

namespace ir {

/// Jump statement. Used to pass control out of the function or inside of a process.
class Jump : public Statement {
public:
    /// Initialize with destination label.
    /// \param _pDestination Destination label.
    /// \param _pLabel Statement label.
    Jump(const LabelPtr &_pDestination = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pDestination(_pDestination) {}

    /// Get statement kind.
    /// \returns #Jump.
    virtual int getKind() const { return JUMP; }

    /// Get destination label.
    /// \return Destination label (cannot be modified).
    const LabelPtr &getDestination() const { return m_pDestination; }

    /// Set destination label.
    /// \param _pDestination Destination label.
    void setDestination(const LabelPtr &_pDestination) { m_pDestination = _pDestination; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Jump(_cloner.get(getDestination()), _cloner.get(getLabel())));
    }

private:
    LabelPtr m_pDestination;
};

/// Assignment statement.
class Assignment : public Statement {
public:
    /// Initialize with lvalue and rvalue.
    /// \param _pLValue LValue.
    /// \param _pExpression RValue.
    /// \param _pLabel Statement label.
    Assignment(const ExpressionPtr &_pLValue = NULL, const ExpressionPtr &_pExpression = NULL, const LabelPtr &_pLabel = NULL)
        : Statement(_pLabel), m_pLValue(_pLValue), m_pExpression(_pExpression) {}

    /// Get statement kind.
    /// \returns #Assignment.
    virtual int getKind() const { return ASSIGNMENT; }

    /// Get left hand side of the assignemnt.
    /// \return Expression that can be assigned to.
    const ExpressionPtr &getLValue() const { return m_pLValue; }

    /// Set left hand side of the assignemnt.
    /// \param _pExpression Expression that can be assigned to.
    void setLValue(const ExpressionPtr &_pExpression) { m_pLValue = _pExpression; }

    /// Get right hand side of the assignemnt.
    /// \return Expression.
    const ExpressionPtr &getExpression() const { return m_pExpression; }

    /// Set right hand side of the assignemnt.
    /// \param _pExpression Expression.
    void setExpression(const ExpressionPtr &_pExpression) { m_pExpression = _pExpression; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Assignment(_cloner.get(getLValue()), _cloner.get(getExpression()), _cloner.get(getLabel())));
    }

private:
    ExpressionPtr m_pLValue, m_pExpression;
};

/// Branch of a predicate call. Contains both output variables and handler pointer.
/// Use methods of Collection to access the list of variables.
/// \extends Node
class CallBranch : public Collection<Expression> {
public:
    /// Default constructor.
    /// \param _pHandler Branch handler statement.
    CallBranch(const StatementPtr &_pHandler = NULL) : m_pHandler(_pHandler) {}

    /// Get branch handler.
    /// \return Branch handler statement.
    const StatementPtr &getHandler() const { return m_pHandler; }

    /// Set branch handler.
    /// \param _pStmt Branch handler statement.
    void setHandler(const StatementPtr &_pStmt) { m_pHandler = _pStmt; }

    virtual NodePtr clone(Cloner &_cloner) const {
        CallBranchPtr pCopy = NEW_CLONE(this, _cloner, CallBranch(_cloner.get(getHandler())));
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }

private:
    StatementPtr m_pHandler;
};

/// Predicate call.
class Call : public Statement {
public:
    /// Default constructor.
    /// \param _pPredicate Expression of predicate type.
    /// \param _pLabel Statement label.
    Call(const ExpressionPtr &_pPredicate = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pPredicate(_pPredicate) {}

    /// Get statement kind.
    /// \returns #Call.
    virtual int getKind() const { return CALL; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    const ExpressionPtr &getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    void setPredicate(const ExpressionPtr &_pExpression) { m_pPredicate = _pExpression; }

    /// Get list of actual parameters.
    /// \return List of expressions used as parameters.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    /// Get list of output branches. Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    Collection<CallBranch> &getBranches() { return m_branches; }

    /// Get list of output branches (const version). Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    const Collection<CallBranch> &getBranches() const { return m_branches; }

    /// Get list of variables declared as part of output parameter list.
    /// \return List of variables.
    Collection<VariableDeclaration> &getDeclarations() { return m_decls; }
    const Collection<VariableDeclaration> &getDeclarations() const { return m_decls; }

    virtual NodePtr clone(Cloner &_cloner) const {
        CallPtr pCopy = NEW_CLONE(this, _cloner, Call(_cloner.get(getPredicate(), true), _cloner.get(getLabel())));
        pCopy->getArgs().appendClones(getArgs(), _cloner);
        pCopy->getBranches().appendClones(getBranches(), _cloner);
        pCopy->getDeclarations().appendClones(getDeclarations(), _cloner);
        return pCopy;
    }

private:
    ExpressionPtr m_pPredicate;
    Collection<Expression> m_args;
    Collection<CallBranch> m_branches;
    Collection<VariableDeclaration> m_decls;
};

/// Multiassignment statement.
/// For constructions like:
/// \code
/// |a, b, c| = |1, 2, 3|;
/// |d, e, f| = foo ();
/// \endcode
/// Note that number of expressions on the left and the right sides may differ
/// due to the use of predicate calls returning more than one result.
class Multiassignment : public Statement {
public:
    /// Default constructor.
    /// \param _pLabel Statement label.
    Multiassignment(const LabelPtr &_pLabel = NULL) : Statement(_pLabel) {}

    /// Get statement kind.
    /// \returns #Multiassignment.
    virtual int getKind() const { return MULTIASSIGNMENT; }

    /// Get list of left hand side expressions (expressions must be assignable).
    /// \return List of expressions.
    Collection<Expression> &getLValues() { return m_LValues; }
    const Collection<Expression> &getLValues() const { return m_LValues; }

    /// Get list of right hand side expressions.
    /// \return List of expressions.
    Collection<Expression> &getExpressions() { return m_expressions; }
    const Collection<Expression> &getExpressions() const { return m_expressions; }

    virtual NodePtr clone(Cloner &_cloner) const {
        MultiassignmentPtr pCopy = NEW_CLONE(this, _cloner, Multiassignment(_cloner.get(getLabel())));
        pCopy->getLValues().appendClones(getLValues(), _cloner);
        pCopy->getExpressions().appendClones(getExpressions(), _cloner);
        return pCopy;
    }

private:
    Collection<Expression> m_LValues;
    Collection<Expression> m_expressions;
};

/// Case-part of switch statement.
class SwitchCase : public Node {
public:
    /// Default constructor.
    /// \param _pStmt Body statement.
    SwitchCase(const StatementPtr &_pBody = NULL) : m_pBody(_pBody) {}

    virtual int getNodeKind() const { return Node::SWITCH_CASE; }

    /// Get list of condition expressions. Can contain ranges.
    /// \return List of expressions.
    Collection<Expression> &getExpressions() { return m_expressions; }
    const Collection<Expression> &getExpressions() const { return m_expressions; }

    /// Get case body statement.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set case body statement.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    virtual NodePtr clone(Cloner &_cloner) const {
        SwitchCasePtr pCopy = NEW_CLONE(this, _cloner, SwitchCase(_cloner.get(getBody())));
        pCopy->getExpressions().appendClones(getExpressions(), _cloner);
        return pCopy;
    }

private:
    StatementPtr m_pBody;
    Collection<Expression> m_expressions;
};

/// Switch statement. Case-parts can be access using methods of Collection.
/// \extends Statement
class Switch : public Collection<SwitchCase, Statement> {
public:
    /// Default constructor.
    /// \param _pArg Argument expression.
    /// \param _pDefault Statement for default alternative.
    /// \param _pDecl Parameter declaration.
    /// \param _pLabel Statement label.
    Switch(const ExpressionPtr &_pArg = NULL, const StatementPtr &_pDefault = NULL, const VariableDeclarationPtr &_pDecl = NULL, const LabelPtr &_pLabel = NULL) :
        m_pArg(_pArg), m_pDefault(_pDefault), m_pDecl(_pDecl)
    {
        setLabel(_pLabel);
    }

    /// Get statement kind.
    /// \returns #Switch.
    virtual int getKind() const { return SWITCH; }

    /// Get parameter declaration (optional).
    /// \return Parameter declaration.
    const VariableDeclarationPtr &getParamDecl() const { return m_pDecl; }

    /// Set parameter declaration (optional).
    /// \param _pDecl Parameter declaration.
    void setParamDecl(const VariableDeclarationPtr &_pDecl) { m_pDecl = _pDecl; }

    /// Get argument expression.
    /// \return Argument expression.
    const ExpressionPtr &getArg() const { return m_pArg; }

    /// Set argument expression.
    /// \param _pExpression Argument expression.
    void setArg(const ExpressionPtr &_pExpression) { m_pArg = _pExpression; }

    /// Get default alternative statement.
    /// \return Statement for default alternative.
    const StatementPtr &getDefault() const { return m_pDefault; }

    /// Set default alternative statement.
    /// \param _pStmt Statement for default alternative.
    void setDefault(const StatementPtr &_pStmt) { m_pDefault = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

    virtual NodePtr clone(Cloner &_cloner) const {
        SwitchPtr pCopy = NEW_CLONE(this, _cloner, Switch(_cloner.get(getArg()), _cloner.get(getDefault()),
                _cloner.get(getParamDecl()), _cloner.get(getLabel())));
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }

private:
    ExpressionPtr m_pArg;
    StatementPtr m_pDefault;
    VariableDeclarationPtr m_pDecl;
};

/// Conditional statement.
class If : public Statement {
public:
    /// Default constructor.
    /// \param _pArg Argument expression.
    /// \param _pBody Body statement.
    /// \param _pElse Else statement.
    /// \param _pLabel Statement label.
    If(const ExpressionPtr &_pArg = NULL, const StatementPtr &_pBody = NULL, const StatementPtr &_pElse = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pArg(_pArg), m_pBody(_pBody), m_pElse(_pElse) {}

    /// Get statement kind.
    /// \returns #If.
    virtual int getKind() const { return IF; }

    /// Get logical argument expression.
    /// \return Argument expression.
    const ExpressionPtr &getArg() const { return m_pArg; }

    /// Set logical argument expression.
    /// \param _pExpression Argument expression.
    void setArg(const ExpressionPtr &_pExpression) { m_pArg = _pExpression; }

    /// Get statement that is executed if condition is true.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set statement that is executed if condition is true.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    /// Get statement that is executed if condition is false.
    /// \return Else statement.
    const StatementPtr &getElse() const { return m_pElse; }

    /// Set statement that is executed if condition is false.
    /// \param _pStmt Else statement.
    void setElse(const StatementPtr &_pStmt) { m_pElse = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getElse())
            return getElse()->isBlockLike();
        else if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, If(_cloner.get(getArg()), _cloner.get(getBody()),
                _cloner.get(getElse()), _cloner.get(getLabel())));
    }

private:
    ExpressionPtr m_pArg;
    StatementPtr m_pBody, m_pElse;
};

/// Imperative for-loop.
class For : public Statement {
public:
    /// Default constructor.
    /// \param _pIterator Iterator variable.
    /// \param _pInvariant Invariant expression.
    /// \param _pIncrement Increment statement.
    /// \param _pBody Body statement.
    /// \param _pLabel Statement label.
    For(const VariableDeclarationPtr &_pIterator = NULL, const ExpressionPtr &_pInvariant = NULL,
            const StatementPtr &_pIncrement = NULL, const StatementPtr &_pBody = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pIterator(_pIterator), m_pInvariant(_pInvariant), m_pIncrement(_pIncrement), m_pBody(_pBody) {}

    /// Get statement kind.
    /// \returns #For.
    virtual int getKind() const { return FOR; }

    /// Get iterator variable.
    /// \return Iterator variable.
    const VariableDeclarationPtr &getIterator() const { return m_pIterator; }

    /// Set iterator variable.
    /// \param _pVar Iterator variable.
    void setIterator(const VariableDeclarationPtr &_pVar) { m_pIterator = _pVar; }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    const ExpressionPtr &getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    void setInvariant(const ExpressionPtr &_pExpression) { m_pInvariant = _pExpression; }

    /// Get iterator increment statement.
    /// \return Increment statement.
    const StatementPtr &getIncrement() const { return m_pIncrement; }

    /// Set iterator increment statement.
    /// \param _pStmt Increment statement.
    void setIncrement(const StatementPtr &_pStmt) { m_pIncrement = _pStmt; }

    /// Get loop body statement.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, For(_cloner.get(getIterator()), _cloner.get(getInvariant()), _cloner.get(getIncrement()),
                _cloner.get(getBody()), _cloner.get(getLabel())));
    }

private:
    VariableDeclarationPtr m_pIterator;
    ExpressionPtr m_pInvariant;
    StatementPtr m_pIncrement, m_pBody;
};

/// Imperative while-loop.
class While : public Statement {
public:
    /// Default constructor.
    /// \param _pInvariant Invariant expression.
    /// \param _pBody Body statement.
    /// \param _pLabel Statement label.
    While(const ExpressionPtr &_pInvariant = NULL, const StatementPtr &_pBody = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pInvariant(_pInvariant), m_pBody(_pBody) {}

    /// Get statement kind.
    /// \returns #While.
    virtual int getKind() const { return WHILE; }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    const ExpressionPtr &getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    void setInvariant(const ExpressionPtr &_pExpression) { m_pInvariant = _pExpression; }

    /// Get loop body statement.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, While(_cloner.get(getInvariant()), _cloner.get(getBody()), _cloner.get(getLabel())));
    }

private:
    ExpressionPtr m_pInvariant;
    StatementPtr m_pBody;
};

/// Imperative break statement. Is used to terminate loop iteration.
class Break : public Statement {
public:
    /// Default constructor.
    /// \param _pLabel Statement label.
    Break(const LabelPtr &_pLabel = NULL) : Statement(_pLabel) {}

    /// Get statement kind.
    /// \returns #Break.
    virtual int getKind() const { return BREAK; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Break(_cloner.get(getLabel())));
    }
};

/// Send message statement.
class Send : public Statement {
public:
    /// Initialize with message and receiver pointers.
    /// \param _pReceiver Message receiver process.
    /// \param _pMessage Message declaration.
    /// \param _pLabel Statement label.
    Send(const ProcessPtr &_pReceiver = NULL, const MessagePtr &_pMessage = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pReceiver(_pReceiver), m_pMessage(_pMessage) {}

    /// Get statement kind.
    /// \returns #Send.
    virtual int getKind() const { return SEND; }

    /// Get message receiver.
    /// \return Receiver process.
    const ProcessPtr &getReceiver() const { return m_pReceiver; }

    /// Set message receiver.
    /// \param _pProcess Message receiver process.
    void setReceiver(const ProcessPtr &_pProcess) { m_pReceiver = _pProcess; }

    /// Get message declaration.
    /// \return Message declaration.
    const MessagePtr &getMessage() const { return m_pMessage; }

    /// Set message declaration pointer.
    /// \param _pMessage Message declaration.
    void setMessage(const MessagePtr &_pMessage) { m_pMessage = _pMessage; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    virtual NodePtr clone(Cloner &_cloner) const {
        SendPtr pCopy = NEW_CLONE(this, _cloner, Send(_cloner.get(getReceiver(), true), _cloner.get(getMessage()), _cloner.get(getLabel())));
        pCopy->getArgs().appendClones(getArgs(), _cloner);
        return pCopy;
    }

private:
    ProcessPtr m_pReceiver;
    MessagePtr m_pMessage;
    Collection<Expression> m_args;
};

/// Handler of incoming message. Part of receive statement.
class MessageHandler : public Node {
public:
    /// Initialize with message pointer.
    /// \param _pMessage Message declaration.
    /// \param _pBody Body statement.
    MessageHandler(const MessagePtr &_pMessage = NULL, const StatementPtr &_pBody = NULL) :
        m_pMessage(_pMessage), m_pBody(_pBody) {}

    virtual int getNodeKind() const { return Node::MESSAGE_HANDLER; }

    /// Get message declaration.
    /// \return Message declaration.
    const MessagePtr &getMessage() const { return m_pMessage; }

    /// Set message declaration pointer.
    /// \param _pMessage Message declaration.
    void setMessage(const MessagePtr &_pMessage) { m_pMessage = _pMessage; }

    /// Get handler body statement.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set handler body statement.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, MessageHandler(_cloner.get(getMessage()), _cloner.get(getBody())));
    }

private:
    MessagePtr m_pMessage;
    StatementPtr m_pBody;
};

/// Receive message statement. \extends Statement
/// This object is a collection of MessageHandler instances, use methods
/// of Collection to access handler definitions.
class Receive : public Collection<MessageHandler, Statement> {
public:
    /// Default constructor.
    /// \param _pTimeout Timeout expression.
    /// \param _pTimeoutStatement Timeout handler.
    /// \param _pLabel Statement label.
    Receive(const ExpressionPtr &_pTimeout = NULL, const StatementPtr &_pTimeoutStatement = NULL, const LabelPtr &_pLabel = NULL) :
        m_pTimeout(_pTimeout), m_pTimeoutStatement(_pTimeoutStatement)
    {
        setLabel(_pLabel);
    }

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return RECEIVE; }

    /// Get timeout expression.
    /// \returns Timeout expression.
    const ExpressionPtr &getTimeout() const { return m_pTimeout; }

    /// Set timeout expression. The expression should be of positive integral type,
    /// it specifies the timeout in milliseconds after which waiting for a message
    /// is stopped and control is passed to timeout handler.
    /// \param _pExpression Timeout expression.
    void setTimeout(const ExpressionPtr &_pExpression) { m_pTimeout = _pExpression; }

    /// Get timeout handler body statement.
    /// \return Timeout handler.
    const StatementPtr &getTimeoutHandler() const { return m_pTimeoutStatement; }

    /// Set timeout handler body statement.
    /// \param _pStmt Timeout handler.
    void setTimeoutHandler(const StatementPtr &_pStmt) { m_pTimeoutStatement = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

    virtual NodePtr clone(Cloner &_cloner) const {
        ReceivePtr pCopy = NEW_CLONE(this, _cloner, Receive(_cloner.get(getTimeout()), _cloner.get(getTimeoutHandler()), _cloner.get(getLabel())));
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }

private:
    ExpressionPtr m_pTimeout;
    StatementPtr m_pTimeoutStatement;
};

/// With statement. Locks variables while executing body.
class With : public Statement {
public:
    /// Default constructor.
    /// \param _pBody Body statement.
    /// \param _pLabel Statement label.
    With(const StatementPtr &_pBody = NULL, const LabelPtr &_pLabel = NULL) : Statement(_pLabel), m_pBody(_pBody) {}

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return WITH; }

    /// Get list of variables to lock.
    /// \return List of assignable expressions.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    /// Get body statement.
    /// \return Body statement.
    const StatementPtr &getBody() const { return m_pBody; }

    /// Set body statement.
    /// \param _pStmt Body statement.
    void setBody(const StatementPtr &_pStmt) { m_pBody = _pStmt; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        WithPtr pCopy = NEW_CLONE(this, _cloner, With(_cloner.get(getBody()), _cloner.get(getLabel())));
        pCopy->getArgs().appendClones(getArgs(), _cloner);
        return pCopy;
    }

private:
    Collection<Expression> m_args;
    StatementPtr m_pBody;
};

} // namespace ir

#endif /* STATEMENTS_H_ */
