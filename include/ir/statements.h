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
class CJump : public CStatement {
public:
    /// Default constructor.
    CJump() : m_pDestination(NULL) {}

    /// Initialize with destination label.
    /// \param _pDestination Destination label.
    CJump(const CLabel * _pDestination) : m_pDestination(_pDestination) {}

    /// Get statement kind.
    /// \returns #Jump.
    virtual int getKind() const { return Jump; }

    /// Get destination label.
    /// \return Destination label (cannot be modified).
    const CLabel * getDestination() const { return m_pDestination; }

    /// Set destination label.
    /// \param _pDestination Destination label.
    void setDestination(const CLabel * _pDestination) { m_pDestination = _pDestination; }

private:
    const CLabel * m_pDestination;
};

/// Assignment statement.
class CAssignment : public CStatement {
public:
    /// Default constructor.
    CAssignment() : m_pLValue(NULL), m_pExpression(NULL) {}

    /// Initialize with lvalue and rvalue.
    /// \param _pLValue LValue.
    /// \param _pExpression RValue.
    /// \param _bReparent If specified (default) also sets parent of _pLValue and _pExpression to this node.
    CAssignment(CExpression * _pLValue = NULL, CExpression * _pExpression = NULL, bool _bReparent = true)
        : m_pLValue(NULL), m_pExpression(NULL)
    {
        _assign(m_pLValue, _pLValue, _bReparent);
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~CAssignment() {
        _delete(m_pLValue);
        _delete(m_pExpression);
    }

    /// Get statement kind.
    /// \returns #Assignment.
    virtual int getKind() const { return Assignment; }

    /// Get left hand side of the assignemnt.
    /// \return Expression that can be assigned to.
    CExpression * getLValue() const { return m_pLValue; }

    /// Set left hand side of the assignemnt.
    /// \param _pExpression Expression that can be assigned to.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setLValue(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pLValue, _pExpression, _bReparent);
    }

    /// Get right hand side of the assignemnt.
    /// \return Expression.
    CExpression * getExpression() const { return m_pExpression; }

    /// Set right hand side of the assignemnt.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    CExpression * m_pLValue, * m_pExpression;
};

/// Branch of a predicate call. Contains both output variables and handler pointer.
/// Use methods of CCollection to access the list of variables.
/// \extends CNode
class CCallBranch : public CCollection<CExpression> {
public:
    /// Default constructor.
    CCallBranch() : m_pHandler(NULL) {}

    /// Destructor.
    virtual ~CCallBranch() { _delete(m_pHandler); }

    /// Get branch handler.
    /// \return Branch handler statement.
    CStatement * getHandler() const { return m_pHandler; }

    /// Set branch handler.
    /// \param _pStmt Branch handler statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setHandler(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pHandler, _pStmt, _bReparent);
    }

private:
    CStatement * m_pHandler;
};

/// Predicate call.
class CCall : public CStatement {
public:
    /// Default constructor.
    CCall() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~CCall() { _delete(m_pPredicate); }

    /// Get statement kind.
    /// \returns #Call.
    virtual int getKind() const { return Call; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    CExpression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of actual parameters.
    /// \return List of expressions used as parameters.
    CCollection<CExpression> & getParams() { return m_params; }
    const CCollection<CExpression> & getParams() const { return m_params; }

    /// Get list of output branches. Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    CCollection<CCallBranch> & getBranches() { return m_branches; }

    /// Get list of output branches (const version). Each branch contain both list of
    /// assignable expressions used as parameters and a branch handler statement.
    /// \return List of branches.
    const CCollection<CCallBranch> & getBranches() const { return m_branches; }

    /// Get list of variables declared as part of output parameter list.
    /// \return List of variables.
    CCollection<CVariableDeclaration> & getDeclarations() { return m_decls; }

private:
    CExpression * m_pPredicate;
    CCollection<CExpression> m_params;
    CCollection<CCallBranch> m_branches;
    CCollection<CVariableDeclaration> m_decls;
};

/// Multiassignment statement.
/// For constructions like:
/// \code
/// |a, b, c| = |1, 2, 3|;
/// |d, e, f| = foo ();
/// \endcode
/// Note that number of expressions on the left and the right sides may differ
/// due to the use of predicate calls returning more than one result.
class CMultiassignment : public CStatement {
public:
    /// Default constructor.
    CMultiassignment() {}

    /// Get statement kind.
    /// \returns #Multiassignment.
    virtual int getKind() const { return Multiassignment; }

    /// Get list of left hand side expressions (expressions must be assignable).
    /// \return List of expressions.
    CCollection<CExpression> & getLValues() { return m_LValues; }

    /// Get list of right hand side expressions.
    /// \return List of expressions.
    CCollection<CExpression> & getExpressions() { return m_expressions; }

private:
    CCollection<CExpression> m_LValues;
    CCollection<CExpression> m_expressions;
};

/// Case-part of switch statement.
class CSwitchCase : public CNode {
public:
    /// Default constructor.
    CSwitchCase() : m_pBody(NULL) {}

    /// Destructor.
    virtual ~CSwitchCase() { _delete(m_pBody); }

    /// Get list of condition expressions. Can contain ranges.
    /// \return List of expressions.
    CCollection<CExpression> & getExpressions() { return m_expressions; }
    const CCollection<CExpression> & getExpressions() const { return m_expressions; }

    /// Get case body statement.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set case body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

private:
    CStatement * m_pBody;
    CCollection<CExpression> m_expressions;
};

/// Switch statement. Case-parts can be access using methods of CCollection.
/// \extends CStatement
class CSwitch : public CCollection<CSwitchCase, CStatement> {
public:
    /// Default constructor.
    CSwitch() : m_pParam(NULL), m_pDefault(NULL), m_pDecl(NULL) {}

    /// Destructor.
    virtual ~CSwitch() {
        _delete(m_pDecl);
        _delete(m_pParam);
        _delete(m_pDefault);
    }

    /// Get statement kind.
    /// \returns #Switch.
    virtual int getKind() const { return Switch; }

    /// Get parameter declaration (optional).
    /// \return Parameter declaration.
    CVariableDeclaration * getParamDecl() const { return m_pDecl; }

    /// Set parameter declaration (optional).
    /// \param _pDecl Parameter declaration.
    /// \param _bReparent If specified (default) also sets parent of _pDecl to this node.
    void setParamDecl(CVariableDeclaration * _pDecl, bool _bReparent = true) {
        _assign(m_pDecl, _pDecl, _bReparent);
    }

    /// Get parameter expression.
    /// \return Parameter expression.
    CExpression * getParam() const { return m_pParam; }

    /// Set parameter expression.
    /// \param _pExpression Parameter expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setParam(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pParam, _pExpression, _bReparent);
    }

    /// Get default alternative statement.
    /// \return Statement for default alternative.
    CStatement * getDefault() const { return m_pDefault; }

    /// Set default alternative statement.
    /// \param _pStmt Statement for default alternative.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setDefault(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pDefault, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

private:
    CExpression * m_pParam;
    CStatement * m_pDefault;
    CVariableDeclaration * m_pDecl;
};

/// Conditional statement.
class CIf : public CStatement {
public:
    /// Default constructor.
    CIf() : m_pParam(NULL), m_pBody(NULL), m_pElse(NULL) {}

    /// Destructor.
    virtual ~CIf() {
        _delete(m_pParam);
        _delete(m_pBody);
        _delete(m_pElse);
    }

    /// Get statement kind.
    /// \returns #If.
    virtual int getKind() const { return If; }

    /// Get logical parameter expression.
    /// \return Parameter expression.
    CExpression * getParam() const { return m_pParam; }

    /// Set logical parameter expression.
    /// \param _pExpression Parameter expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setParam(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pParam, _pExpression, _bReparent);
    }

    /// Get statement that is executed if condition is true.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set statement that is executed if condition is true.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

    /// Get statement that is executed if condition is false.
    /// \return Else statement.
    CStatement * getElse() const { return m_pElse; }

    /// Set statement that is executed if condition is false.
    /// \param _pStmt Else statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setElse(CStatement * _pStmt, bool _bReparent = true) {
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
    CExpression * m_pParam;
    CStatement * m_pBody, * m_pElse;
};

/// Imperative for-loop.
class CFor : public CStatement {
public:
    /// Default constructor.
    CFor() : m_pIterator(NULL), m_pInvariant(NULL), m_pIncrement(NULL), m_pBody(NULL) {}

    /// Destructor.
    virtual ~CFor() {
        _delete(m_pIterator);
        _delete(m_pInvariant);
        _delete(m_pIncrement);
        _delete(m_pBody);
    }

    /// Get statement kind.
    /// \returns #For.
    virtual int getKind() const { return For; }

    /// Get iterator variable.
    /// \return Iterator variable.
    CVariableDeclaration * getIterator() const { return m_pIterator; }

    /// Set iterator variable.
    /// \param _pVar Iterator variable.
    /// \param _bReparent If specified (default) also sets parent of _pVar to this node.
    void setIterator(CVariableDeclaration * _pVar, bool _bReparent = true) {
        _assign(m_pIterator, _pVar, _bReparent);
    }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    CExpression * getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setInvariant(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pInvariant, _pExpression, _bReparent);
    }

    /// Get iterator increment statement.
    /// \return Increment statement.
    CStatement * getIncrement() const { return m_pIncrement; }

    /// Set iterator increment statement.
    /// \param _pStmt Increment statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setIncrement(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pIncrement, _pStmt, _bReparent);
    }

    /// Get loop body statement.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
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
    CVariableDeclaration * m_pIterator;
    CExpression * m_pInvariant;
    CStatement * m_pIncrement, * m_pBody;
};

/// Imperative while-loop.
class CWhile : public CStatement {
public:
    /// Default constructor.
    CWhile() : m_pInvariant(NULL), m_pBody(NULL) {}

    /// Destructor.
    virtual ~CWhile() {
        _delete(m_pInvariant);
        _delete(m_pBody);
    }

    /// Get statement kind.
    /// \returns #While.
    virtual int getKind() const { return While; }

    /// Get loop invariant expression.
    /// \return Invariant expression.
    CExpression * getInvariant() const { return m_pInvariant; }

    /// Set loop invariant expression.
    /// \param _pExpression Invariant expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setInvariant(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pInvariant, _pExpression, _bReparent);
    }

    /// Get loop body statement.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set loop body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
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
    CExpression * m_pInvariant;
    CStatement * m_pBody;
};

/// Imperative break statement. Is used to terminate loop iteration.
class CBreak : public CStatement {
public:
    /// Default constructor.
    CBreak() {}

    /// Get statement kind.
    /// \returns #Break.
    virtual int getKind() const { return Break; }
};

/// Send message statement.
class CSend : public CStatement {
public:
    /// Default constructor.
    CSend() : m_pReceiver(NULL), m_pMessage(NULL) {}

    /// Initialize with message and receiver pointers.
    /// \param _pProcess Message receiver process.
    /// \param _pMessage Message definition.
    CSend(const CProcess * _pProcess, const CMessage * _pMessage)
        : m_pReceiver(_pProcess), m_pMessage(_pMessage) {}

    /// Get statement kind.
    /// \returns #Send.
    virtual int getKind() const { return Send; }

    /// Get message receiver.
    /// \return Receiver process.
    const CProcess * getReceiver() const { return m_pReceiver; }

    /// Set message receiver.
    /// \param _pProcess Message receiver process.
    void setReceiver(const CProcess * _pProcess) { m_pReceiver = _pProcess; }

    /// Get message definition.
    /// \return Message definition.
    const CMessage * getMessage() const { return m_pMessage; }

    /// Set message definition pointer.
    /// \param _pMessage Message definition.
    void setMessage(const CMessage * _pMessage) { m_pMessage = _pMessage; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    CCollection<CExpression> & getParams() { return m_params; }

private:
    const CProcess * m_pReceiver;
    const CMessage * m_pMessage;
    CCollection<CExpression> m_params;
};

/// Handler of incoming message. Part of receive statement.
class CMessageHandler : public CNode {
public:
    /// Default constructor.
    CMessageHandler() : m_pMessage(NULL), m_pBody(NULL) {}

    /// Initialize with message pointer.
    /// \param _pMessage Message definition.
    CMessageHandler(const CMessage * _pMessage) : m_pMessage(_pMessage), m_pBody(NULL) {}

    /// Destructor.
    virtual ~CMessageHandler() { _delete(m_pBody); }

    /// Get message definition.
    /// \return Message definition.
    const CMessage * getMessage() const { return m_pMessage; }

    /// Set message definition pointer.
    /// \param _pMessage Message definition.
    void setMessage(const CMessage * _pMessage) { m_pMessage = _pMessage; }

    /// Get handler body statement.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set handler body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pBody, _pStmt, _bReparent);
    }

private:
    const CMessage * m_pMessage;
    CStatement * m_pBody;
};

/// Receive message statement. \extends CStatement
/// This object is a collection of CMessageHandler instances, use methods
/// of CCollection to access handler definitions.
class CReceive : public CCollection<CMessageHandler, CStatement> {
public:
    /// Default constructor.
    CReceive() : m_pTimeout(NULL), m_pTimeoutStatement(NULL) {}

    /// Destructor.
    virtual ~CReceive() {
        _delete(m_pTimeout);
        _delete(m_pTimeoutStatement);
    }

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return Receive; }

    /// Get timeout expression.
    /// \returns Timeout expression.
    CExpression * getTimeout() const { return m_pTimeout; }

    /// Set timeout expression. The expression should be of positive integral type,
    /// it specifies the timeout in milliseconds after which waiting for a message
    /// is stopped and control is passed to timeout handler.
    /// \param _pExpression Timeout expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setTimeout(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pTimeout, _pExpression, _bReparent);
    }

    /// Get timeout handler body statement.
    /// \return Timeout handler.
    CStatement * getTimeoutHandler() const { return m_pTimeoutStatement; }

    /// Set timeout handler body statement.
    /// \param _pStmt Timeout handler.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setTimeoutHandler(CStatement * _pStmt, bool _bReparent = true) {
        _assign(m_pTimeoutStatement, _pStmt, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return true; }

private:
    CExpression * m_pTimeout;
    CStatement * m_pTimeoutStatement;
};

/// With statement. Locks variables while executing body.
class CWith : public CStatement {
public:
    /// Default constructor.
    CWith() : m_pBody(NULL) {}

    /// Destructor.
    virtual ~CWith() { _delete(m_pBody); }

    /// Get statement kind.
    /// \returns #Receive.
    virtual int getKind() const { return With; }

    /// Get list of variables to lock.
    /// \return List of assignable expressions.
    CCollection<CExpression> & getParams() { return m_params; }

    /// Get body statement.
    /// \return Body statement.
    CStatement * getBody() const { return m_pBody; }

    /// Set body statement.
    /// \param _pStmt Body statement.
    /// \param _bReparent If specified (default) also sets parent of _pStmt to this node.
    void setBody(CStatement * _pStmt, bool _bReparent = true) {
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
    CCollection<CExpression> m_params;
    CStatement * m_pBody;
};

} // namespace ir

#endif /* STATEMENTS_H_ */
