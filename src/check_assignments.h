/// \file check_assignments.h
///

#ifndef CHECK_ASSIGNMENTS_H_
#define CHECK_ASSIGNMENTS_H_

#include "ir/declarations.h"
#include "ir/visitor.h"

namespace ir {

// Check that only variables are LValues in assignments.
class CheckAssignments : public Visitor {
public:
    virtual bool visitAssignment(Assignment &_node) {
        if (_node.getLValue()->getKind() != Expression::VAR)
            throw std::runtime_error("Only variables are supported in assignments.");

        return true;
    }

    virtual bool visitMultiassignment(Multiassignment &_node) {
        for (size_t i = 0; i < _node.getLValues().size(); ++i)
            if (_node.getLValues().get(i)->getKind() != Expression::VAR)
                throw std::runtime_error("Only variables are supported in multiassignments.");

        return true;
    }
};

}


#endif /* CHECK_ASSIGNMENTS_H_ */
