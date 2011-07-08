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
    /// Default constructor.
    Jump() : m_pDestination(NULL) {}

    /// Initialize with destination label.
    /// \param _pDestination Destination label.
    Jump(const LabelPtr &_pDestination) : m_pDestination(_pDestination) {}

    /// Get statement kind.
    /// \returns #Jump.
    virtual int getKind() const { return JUMP; }

    /// Get destination label.
    /// \return Destination label (cannot be modified).
    const LabelPtr &getDestination() const { return m_pDestination; }

    /// Set destination label.
    /// \param _pDestination Destination label.
    void setDestination(const LabelPtr &_pDestination) { m_pDestination = _pDestination; }

private:
    LabelPtr m_pDestination;
};

/// Assignment statement.
class Assignment : public Statement {
public:
    /// Default constructor.
    Assignment() : m_pLValue(NULL), m_pExpression(NULL) {}

    /// Initialize with lvalue and rvalue.
    /// \param _pLValue LValue.
    /// \param _pExpression RValue.
    Assignment(const ExpressionPtr &_pLValue = NULL, const ExpressionPtr &_pExpression = NULL)
        : m_pLValue(_pLValue), m_pExpression(_pExpression) {}

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

private:
    ExpressionPtr m_pLValue, m_pExpression;
};

/// Branch of a predicate call. Contains both output variables and handler pointer.
/// Use methods of Collection to access the list of variables.
/// \extends Node
class CallBranch : public Collection<Expression> {
public:
    /// Default constructor.
    CallBranch() : m_pHandler(NULL) {}

    /// Get branch handler.
    /// \return Branch handler statement.
    const StatementPtr &getHandler() const { return m_pHandler; }

    /// Set branch handler.
    /// \param _pStmt Branch handler statement.
    void setHandler(const StatementPtr &_pStmt) { m_pHandler = _pStmt; }

private:
    StatementPtr m_pHandler;
};

/// Predicate call.
class Call : public Statement {
public:
    /// Default constructor.
    Call() : m_pPredicate(NULL) {}

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
    Multiassignment() {}

    /// Get statement kind.
    /// \returns #Multiassignment.
    virtual int getKind() const { return MULTIASSIGNMENT; }

    /// Get list of left hand side expressions (expressions must be assignable).
    /// \return List of expressions.
    Collection<Expression> &getLValues() { return m_LValues; }

    /// Get list of right hand side expressions.
    /// \return List of expressions.
    Collection<Expression> &getExpressions() { return m_expressions; }

private:
    Collection<Expression> m_LValues;
    Collection<Expression> m_expressions;
};

/// Case-part of switch statement.
class SwitchCase : public Node {
public:
    /// Default constructor.
    SwitchCase() : m_pBody(NULL) {}

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

private:
    StatementPtr m_pBody;
    Collection<Expression> m_expressions;
};

/// Switch statement. Case-parts can be access using methods of Collection.
/// \extends Statement
class Switch : public Collection<SwitchCase, Statement> {
public:
    /// Default constructor.
    Switch() : m_pArg(NULL), m_pDefault(NULL), m_pDecl(NULL) {}

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

private:
    ExpressionPtr m_pArg;
    StatementPtr m_pDefault;
    VariableDeclarationPtr m_pDecl;
};

/// Conditional statement.
class If : public Statement {
public:
    /// Default constructor.
    If() : m_pArg(NULL), m_pBody(NULL), m_pElse(NULL) {}

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

private:
    ExpressionPtr m_pArg;
    StatementPtr m_pBody, m_pElse;
};

/// Imperative for-loop.
class For : public Statement {
public:
    /// Default constructor.
    For() : m_pIterator(NULL), m_pInvariant(NULL), m_pIncrement(NULL), m_pBody(NULL) {}

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

private:
    VariableDeclarationPtr m_pIterator;
    ExpressionPtr m_pInvariant;
    StatementPtr m_pIncrement, m_pBody;
};

/// Imperative while-loop.
class While : public Statement {
public:
    /// Default constructor.
    While() : m_pInvariant(NULL), m_pBody(NULL) {}

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

private:
    ExpressionPtr m_pInvariant;
    StatementPtr m_pBody;
};

/// Imperative break statement. Is used to terminate loop iteration.
class Break : public Statement {
public:
    /// Default constructor.
    Break() {}

    /// Get statement kind.
    /// \returns #Break.
    virtual int getKind() const { return BREAK; }
};

/// Send message statement.
class Send : public Statement {
public:
    /// Default constructor.
    Send() : m_pReceiver(NULL), m_pMessage(NULL) {}

    /// Initialize with message and receiver pointers.
    /// \param _pProcess Message receiver process.
    /// \param _pMessage Message declaration.
    Send(const ProcessPtr &_pProcess, const MessagePtr &_pMessage)
        : m_pReceiver(_pProcess), m_pMessage(_pMessage) {}

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

private:
    ProcessPtr m_pReceiver;
    MessagePtr m_pMessage;
    Collection<Expression> m_args;
};

/// Handler of incoming message. Part of receive statement.
class MessageHandler : public Node {
public:
    /// Default constructor.
    MessageHandler() : m_pMessage(NULL), m_pBody(NULL) {}

    /// Initialize with message pointer.
    /// \param _pMessage Message declaration.
    MessageHandler(const MessagePtr &_pMessage) : m_pMessage(_pMessage), m_pBody(NULL) {}

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
    Receive() : m_pTimeout(NULL), m_pTimeoutStatement(NULL) {}

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

private:
    ExpressionPtr m_pTimeout;
    StatementPtr m_pTimeoutStatement;
};

/// With statement. Locks variables while executing body.
class With : public Statement {
public:
    /// Default constructor.
    With() : m_pBody(NULL) {}

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return WITH; }

    /// Get list of variables to lock.
    /// \return List of assignable expressions.
    Collection<Expression> &getArgs() { return m_args; }

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

private:
    Collection<Expression> m_args;
    StatementPtr m_pBody;
};

} // namespace ir

#endif /* STATEMENTS_H_ */
