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
    Jump(const Label * _pDestination) : m_pDestination(_pDestination) {}

    /// Get statement kind.
    /// \returns #Jump.
    virtual int getKind() const { return JUMP; }

    /// Get destination label.
    /// \return Destination label (cannot be modified).
    const Label * getDestination() const { return m_pDestination; }

    /// Set destination label.
    /// \param _pDestination Destination label.
    void setDestination(const Label * _pDestination) { m_pDestination = _pDestination; }

private:
    const Label * m_pDestination;
};

/// Assignment statement.
class Assignment : public Statement {
public:
    /// Default constructor.
    Assignment() : m_pLValue(NULL), m_pExpression(NULL) {}

    /// Initialize with lvalue and rvalue.
    /// \param _pLValue LValue.
    /// \param _pExpression RValue.
    /// \param _bReparent If specified (default) also sets parent of _pLValue and _pExpression to this node.
    Assignment(Expression * _pLValue = NULL, Expression * _pExpression = NULL, bool _bReparent = true)
        : m_pLValue(NULL), m_pExpression(NULL)
    {
        _assign(m_pLValue, _pLValue, _bReparent);
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~Assignment() {
        _delete(m_pLValue);
        _delete(m_pExpression);
    }

    /// Get statement kind.
    /// \returns #Assignment.
    virtual int getKind() const { return ASSIGNMENT; }

    /// Get left hand side of the assignemnt.
    /// \return Expression that can be assigned to.
    Expression * getLValue() const { return m_pLValue; }

    /// Set left hand side of the assignemnt.
    /// \param _pExpression Expression that can be assigned to.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setLValue(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pLValue, _pExpression, _bReparent);
    }

    /// Get right hand side of the assignemnt.
    /// \return Expression.
    Expression * getExpression() const { return m_pExpression; }

    /// Set right hand side of the assignemnt.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    Expression * m_pLValue, * m_pExpression;
};

/// Branch of a predicate call. Contains both output variables and handler pointer.
/// Use methods of Collection to access the list of variables.
/// \extends Node
class CallBranch : public Collection<Expression> {
public:
    /// Default constructor.
    CallBranch() : m_pHandler(NULL) {}

    /// Destructor.
    virtual ~CallBranch() { _delete(m_pHandler); }

    /// Get branch handler.
    /// \return Branch handler statement.
    Statement * getHandler() const { return m_pHandler; }

    /// Set branch handler.
    /// \param _pStmt Branch handler statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setHandler(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pHandler, _pStmt, _bReparent);
    }

private:
    Statement * m_pHandler;
};

/// Predicate call.
class Call : public Statement {
public:
    /// Default constructor.
    Call() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~Call() { _delete(m_pPredicate); }

    /// Get statement kind.
    /// \returns #Call.
    virtual int getKind() const { return CALL; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    Expression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of actual parameters.
    /// \return List of expressions used as parameters.
    Collection<Expression> & getArgs() { return m_args; }
    const Collection<Expression> & getArgs() const { return m_args; }

    /// Get list of output branches. Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    Collection<CallBranch> & getBranches() { return m_branches; }

    /// Get list of output branches (const version). Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    const Collection<CallBranch> & getBranches() const { return m_branches; }

    /// Get list of variables declared as part of output parameter list.
    /// \return List of variables.
    Collection<VariableDeclaration> & getDeclarations() { return m_decls; }
    const Collection<VariableDeclaration> & getDeclarations() const { return m_decls; }

private:
    Expression * m_pPredicate;
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
    Collection<Expression> & getLValues() { return m_LValues; }

    /// Get list of right hand side expressions.
    /// \return List of expressions.
    Collection<Expression> & getExpressions() { return m_expressions; }

private:
    Collection<Expression> m_LValues;
    Collection<Expression> m_expressions;
};

/// Case-part of switch statement.
class SwitchCase : public Node {
public:
    /// Default constructor.
    SwitchCase() : m_pBody(NULL) {}

    /// Destructor.
    virtual ~SwitchCase() { _delete(m_pBody); }

    virtual int getNodeKind() const { return Node::SWITCH_CASE; }

    /// Get list of condition expressions. Can contain ranges.
    /// \return List of expressions.
    Collection<Expression> & getExpressions() { return m_expressions; }
    const Collection<Expression> & getExpressions() const { return m_expressions; }

    /// Get case body statement.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set case body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

private:
    Statement * m_pBody;
    Collection<Expression> m_expressions;
};

/// Switch statement. Case-parts can be access using methods of Collection.
/// \extends Statement
class Switch : public Collection<SwitchCase, Statement> {
public:
    /// Default constructor.
    Switch() : m_pArg(NULL), m_pDefault(NULL), m_pDecl(NULL) {}

    /// Destructor.
    virtual ~Switch() {
        _delete(m_pDecl);
        _delete(m_pArg);
        _delete(m_pDefault);
    }

    /// Get statement kind.
    /// \returns #Switch.
    virtual int getKind() const { return SWITCH; }

    /// Get parameter declaration (optional).
    /// \return Parameter declaration.
    VariableDeclaration * getParamDecl() const { return m_pDecl; }

    /// Set parameter declaration (optional).
    /// \param _pDecl Parameter declaration.
    /// \param _bReparent If specified (default) also sets parent of _pDecl to this node.
    void setParamDecl(VariableDeclaration * _pDecl, bool _bReparent = true) {
        _assign(m_pDecl, _pDecl, _bReparent);
    }

    /// Get argument expression.
    /// \return Argument expression.
    Expression * getArg() const { return m_pArg; }

    /// Set argument expression.
    /// \param _pExpression Argument expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setArg(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pArg, _pExpression, _bReparent);
    }

    /// Get default alternative statement.
    /// \return Statement for default alternative.
    Statement * getDefault() const { return m_pDefault; }

    /// Set default alternative statement.
    /// \param _pStmt Statement for default alternative.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setDefault(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pDefault, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

private:
    Expression * m_pArg;
    Statement * m_pDefault;
    VariableDeclaration * m_pDecl;
};

/// Conditional statement.
class If : public Statement {
public:
    /// Default constructor.
    If() : m_pArg(NULL), m_pBody(NULL), m_pElse(NULL) {}

    /// Destructor.
    virtual ~If() {
        _delete(m_pArg);
        _delete(m_pBody);
        _delete(m_pElse);
    }

    /// Get statement kind.
    /// \returns #If.
    virtual int getKind() const { return IF; }

    /// Get logical argument expression.
    /// \return Argument expression.
    Expression * getArg() const { return m_pArg; }

    /// Set logical argument expression.
    /// \param _pExpression Argument expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setArg(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pArg, _pExpression, _bReparent);
    }

    /// Get statement that is executed if condition is true.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set statement that is executed if condition is true.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

    /// Get statement that is executed if condition is false.
    /// \return Else statement.
    Statement * getElse() const { return m_pElse; }

    /// Set statement that is executed if condition is false.
    /// \param _pStmt Else statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setElse(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pElse, _pStmt, _bReparent);
    }

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
    Expression * m_pArg;
    Statement * m_pBody, * m_pElse;
};

/// Imperative for-loop.
class For : public Statement {
public:
    /// Default constructor.
    For() : m_pIterator(NULL), m_pInvariant(NULL), m_pIncrement(NULL), m_pBody(NULL) {}

    /// Destructor.
    virtual ~For() {
        _delete(m_pIterator);
        _delete(m_pInvariant);
        _delete(m_pIncrement);
        _delete(m_pBody);
    }

    /// Get statement kind.
    /// \returns #For.
    virtual int getKind() const { return FOR; }

    /// Get iterator variable.
    /// \return Iterator variable.
    VariableDeclaration * getIterator() const { return m_pIterator; }

    /// Set iterator variable.
    /// \param _pVar Iterator variable.
    /// \param _bReparent If specified (default) also sets parent of _pVar to this node.
    void setIterator(VariableDeclaration * _pVar, bool _bReparent = true) {
        _assign(m_pIterator, _pVar, _bReparent);
    }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    Expression * getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setInvariant(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pInvariant, _pExpression, _bReparent);
    }

    /// Get iterator increment statement.
    /// \return Increment statement.
    Statement * getIncrement() const { return m_pIncrement; }

    /// Set iterator increment statement.
    /// \param _pStmt Increment statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setIncrement(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pIncrement, _pStmt, _bReparent);
    }

    /// Get loop body statement.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

private:
    VariableDeclaration * m_pIterator;
    Expression * m_pInvariant;
    Statement * m_pIncrement, * m_pBody;
};

/// Imperative while-loop.
class While : public Statement {
public:
    /// Default constructor.
    While() : m_pInvariant(NULL), m_pBody(NULL) {}

    /// Destructor.
    virtual ~While() {
        _delete(m_pInvariant);
        _delete(m_pBody);
    }

    /// Get statement kind.
    /// \returns #While.
    virtual int getKind() const { return WHILE; }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    Expression * getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setInvariant(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pInvariant, _pExpression, _bReparent);
    }

    /// Get loop body statement.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const {
        if (getBody())
            return getBody()->isBlockLike();
        else
            return false;
    }

private:
    Expression * m_pInvariant;
    Statement * m_pBody;
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
    Send(const Process * _pProcess, const Message * _pMessage)
        : m_pReceiver(_pProcess), m_pMessage(_pMessage) {}

    /// Get statement kind.
    /// \returns #Send.
    virtual int getKind() const { return SEND; }

    /// Get message receiver.
    /// \return Receiver process.
    const Process * getReceiver() const { return m_pReceiver; }

    /// Set message receiver.
    /// \param _pProcess Message receiver process.
    void setReceiver(const Process * _pProcess) { m_pReceiver = _pProcess; }

    /// Get message declaration.
    /// \return Message declaration.
    const Message * getMessage() const { return m_pMessage; }

    /// Set message declaration pointer.
    /// \param _pMessage Message declaration.
    void setMessage(const Message * _pMessage) { m_pMessage = _pMessage; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> & getArgs() { return m_args; }

private:
    const Process * m_pReceiver;
    const Message * m_pMessage;
    Collection<Expression> m_args;
};

/// Handler of incoming message. Part of receive statement.
class MessageHandler : public Node {
public:
    /// Default constructor.
    MessageHandler() : m_pMessage(NULL), m_pBody(NULL) {}

    /// Initialize with message pointer.
    /// \param _pMessage Message declaration.
    MessageHandler(const Message * _pMessage) : m_pMessage(_pMessage), m_pBody(NULL) {}

    /// Destructor.
    virtual ~MessageHandler() { _delete(m_pBody); }

    virtual int getNodeKind() const { return Node::MESSAGE_HANDLER; }

    /// Get message declaration.
    /// \return Message declaration.
    const Message * getMessage() const { return m_pMessage; }

    /// Set message declaration pointer.
    /// \param _pMessage Message declaration.
    void setMessage(const Message * _pMessage) { m_pMessage = _pMessage; }

    /// Get handler body statement.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set handler body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

private:
    const Message * m_pMessage;
    Statement * m_pBody;
};

/// Receive message statement. \extends Statement
/// This object is a collection of MessageHandler instances, use methods
/// of Collection to access handler definitions.
class Receive : public Collection<MessageHandler, Statement> {
public:
    /// Default constructor.
    Receive() : m_pTimeout(NULL), m_pTimeoutStatement(NULL) {}

    /// Destructor.
    virtual ~Receive() {
        _delete(m_pTimeout);
        _delete(m_pTimeoutStatement);
    }

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return RECEIVE; }

    /// Get timeout expression.
    /// \returns Timeout expression.
    Expression * getTimeout() const { return m_pTimeout; }

    /// Set timeout expression. The expression should be of positive integral type,
    /// it specifies the timeout in milliseconds after which waiting for a message
    /// is stopped and control is passed to timeout handler.
    /// \param _pExpression Timeout expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setTimeout(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pTimeout, _pExpression, _bReparent);
    }

    /// Get timeout handler body statement.
    /// \return Timeout handler.
    Statement * getTimeoutHandler() const { return m_pTimeoutStatement; }

    /// Set timeout handler body statement.
    /// \param _pStmt Timeout handler.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setTimeoutHandler(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pTimeoutStatement, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

private:
    Expression * m_pTimeout;
    Statement * m_pTimeoutStatement;
};

/// With statement. Locks variables while executing body.
class With : public Statement {
public:
    /// Default constructor.
    With() : m_pBody(NULL) {}

    /// Destructor.
    virtual ~With() { _delete(m_pBody); }

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return WITH; }

    /// Get list of variables to lock.
    /// \return List of assignable expressions.
    Collection<Expression> & getArgs() { return m_args; }

    /// Get body statement.
    /// \return Body statement.
    Statement * getBody() const { return m_pBody; }

    /// Set body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(Statement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

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
    Statement * m_pBody;
};

} // namespace ir

#endif /* STATEMENTS_H_ */
