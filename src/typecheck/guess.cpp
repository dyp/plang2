/// \file guess.cpp
///

#include "operations.h"

#include <list>

namespace tc {

class Guess : public OperationOnLattice {
public:
    Guess() {}

private:
    size_t m_uLowersCount, m_uUppersCount;

    bool _handler(const ir::TypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers);

    ir::TypePtr _matchSingleUpperBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers);
    ir::TypePtr _matchSingleLowerBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers);
    ir::TypePtr _matchEqualizableUpperBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers);
};

bool Guess::_handler(const ir::TypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) {
    const tc::FreshTypePtr pType = _pType.as<tc::FreshType>();

    m_uLowersCount = _lowers.size() > 0 ? _getExtraLowerBoundsCount(_pType) + _lowers.size() : 0;
    m_uUppersCount = _uppers.size() > 0 ? _getExtraUpperBoundsCount(_pType) + _uppers.size() : 0;

    ir::TypePtr
        pUpper = _matchSingleUpperBound(pType, _lowers, _uppers),
        pLower = _matchSingleLowerBound(pType, _lowers, _uppers);

    if (!pUpper && !pLower && _getExtraUpperBoundsCount(pType) == 0)
        pUpper = _matchEqualizableUpperBound(pType, _lowers, _uppers);

    // Types with both flags set cannot choose between non-fresh bounds.
    if (pType->getFlags() == (tc::FreshType::PARAM_IN | tc::FreshType::PARAM_OUT)) {
        if (pUpper && pUpper->getKind() != ir::Type::FRESH && m_uLowersCount > 0)
            pUpper = nullptr;
        if (pLower && pLower->getKind() != ir::Type::FRESH && m_uUppersCount > 0)
            pLower = nullptr;
    }

    const ir::TypePtr pOther =
        pUpper ? pUpper : pLower;

    return (pOther ? _context().add(new tc::Formula(tc::Formula::EQUALS, pType, pOther)) : false);
}

// Matching the following patterns:
//    A1,k(IN) <= *n,m   ->  A*n,m+k(IN)
//    *p,k <= An,1(OUT)  ->  *An+p,k(OUT)
// where A are fresh type; 1, k, m, n, p are infs/sups count; * is any type.
ir::TypePtr Guess::_matchSingleUpperBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) {
    if ((_pType->getFlags() & tc::FreshType::PARAM_IN) != 0 && m_uUppersCount == 1 && !(*_uppers.begin())->isStrict())
        return _uppers.getType(_uppers.begin());
    if (m_uUppersCount == 1 && !(*_uppers.begin())->isStrict() && _uppers.getType(_uppers.begin())->getKind() == ir::Type::FRESH &&
        (_uppers.getType(_uppers.begin()).as<tc::FreshType>()->getFlags() & tc::FreshType::PARAM_OUT) != 0)
        return _uppers.getType(_uppers.begin());
    return nullptr;
}

// Matching the following patterns:
//    Bp,k(IN) <= An,1   ->  BAn+p,k(IN)
//    A1,k <= Bn,m(OUT)  ->  ABn,m+k(OUT)
// where A, B are fresh types; 1, k, m, n, p are infs/sups count; * is any type.
ir::TypePtr Guess::_matchSingleLowerBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) {
    if (m_uLowersCount == 1 && !(*_lowers.begin())->isStrict() && _lowers.getType(_lowers.begin())->getKind() == ir::Type::FRESH &&
        (_lowers.getType(_lowers.begin()).as<tc::FreshType>()->getFlags() & tc::FreshType::PARAM_IN) != 0)
        return _lowers.getType(_lowers.begin());
    if ((_pType->getFlags() & tc::FreshType::PARAM_OUT) != 0 && m_uLowersCount == 1 && !(*_lowers.begin())->isStrict())
        return _lowers.getType(_lowers.begin());
    return nullptr;
}

// Matching the following patterns:
//    A2,0(IN) <= B0,2(OUT)  ->  AB1,1
// ensure that sup and inf holds order.
ir::TypePtr Guess::_matchEqualizableUpperBound(const tc::FreshTypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) {
    if (!(_pType->getFlags() & tc::FreshType::PARAM_IN) || _uppers.size() != 2)
        return nullptr;

    ir::TypePtr
        pSup1 = _uppers.getType(_uppers.begin()),
        pSup2 = _uppers.getType(std::next(_uppers.begin(), 1));

    auto canBeUpper = [&](ir::TypePtr _pSomeType) {
        return _pSomeType->getKind() == ir::Type::FRESH &&
            (_pSomeType.as<tc::FreshType>()->getFlags() & tc::FreshType::PARAM_OUT) &&
            _context().pTypes->lowers(_pSomeType).size() == 2;
    };

    std::list<std::pair<ir::TypePtr, ir::TypePtr>> candidates;

    if (canBeUpper(pSup1))
        candidates.push_back({pSup1, pSup2});

    if (canBeUpper(pSup2))
        candidates.push_back({pSup2, pSup1});

    for (auto &candidate : candidates) {
        const ir::TypePtr
            pFresh = candidate.first,
            pSup = clone(candidate.second);

        const auto& lowers = _context().pTypes->lowers(pFresh);

        // pInf <= (pFresh => pType) <= pSup
        ir::TypePtr pInf = lowers.getType(lowers.begin());
        if (pInf == _pType)
            pInf = lowers.getType(std::next(lowers.begin(), 1));

        pInf = clone(pInf);

        pSup->rewrite(_pType, pFresh);
        pInf->rewrite(_pType, pFresh);

        tc::RelationPtr pRelation = new tc::Relation(tc::Formula(tc::Formula::SUBTYPE, pInf, pSup));

        auto &relations = _context().pTypes->relations();

        // Ensure that pInf <= pSup
        if (pRelation->eval() == tc::Formula::TRUE ||
            relations.find(pRelation) != relations.end()) {
            pFresh.as<tc::FreshType>()->setFlags(0);
            return pFresh;
        }
    }

    return nullptr;
}

Auto<Operation> Operation::guess() {
    return new Guess();
}

}
