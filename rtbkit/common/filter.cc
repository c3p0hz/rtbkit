/** filter.cc                                 -*- C++ -*-
    Rémi Attab, 24 Jul 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Implementation of the filter registry.

*/

#include "filter.h"
#include "rtbkit/common/bid_request.h"
#include "jml/arch/spinlock.h"
#include "jml/utils/exc_check.h"

#include <unordered_map>
#include <mutex>


using namespace std;
using namespace ML;


namespace RTBKIT {


/******************************************************************************/
/* FILTER STATE                                                               */
/******************************************************************************/

std::unordered_map<unsigned, BiddableSpots>
FilterState::
biddableSpots()
{
    // Used to remove creatives for configs that have been filtered out.
    narrowAllCreatives(CreativeMatrix(configs_));

    std::unordered_map<unsigned, BiddableSpots> biddable;

    for (size_t impId = 0; impId < creatives_.size(); ++impId) {
        // \todo Would be nice if we could remove this temp map.
        std::unordered_map<unsigned, SmallIntVector> biddableCreatives;

        for (unsigned crId = 0; crId < creatives_[impId].size(); ++crId) {
            const auto& configs = creatives_[impId][crId];

            for (size_t config = configs.next();
                 config < configs.size();
                 config = configs.next(config + 1))
            {
                biddableCreatives[config].push_back(crId);
            }
        }

        for (const auto& entry : biddableCreatives)
            biddable[entry.first].emplace_back(impId, entry.second);
    }

    return biddable;
}

FilterState::FilterReasons&
FilterState::getFilterReasons(){
    return this->filterReasons_;
}

void
FilterState::resetFilterReasons(){
    this->filterReasons_.clear();
}

/******************************************************************************/
/* FILTER REGISTRY                                                            */
/******************************************************************************/

namespace {

struct FilterRegister {
    unordered_map<string, FilterRegistry::ConstructFn> filterRegister;
    Spinlock filterRegisterLock;
};

FilterRegister * filters ;

FilterRegister * getFilterRegister() {
    if(!filters) {
        filters = new FilterRegister();
    }
    return filters;
}


} // namespace anonymous


void
FilterRegistry::
registerFilter(const string& name, ConstructFn fn)
{
    auto filters = getFilterRegister();
    lock_guard<Spinlock> guard(filters->filterRegisterLock);
    filters->filterRegister.insert(make_pair(name, fn));
}


FilterBase*
FilterRegistry::
makeFilter(const string& name)
{
    ConstructFn fn;
    auto filters = getFilterRegister();
    {
        lock_guard<Spinlock> guard(filters->filterRegisterLock);

        auto it = filters->filterRegister.find(name);
        ExcCheck(it != filters->filterRegister.end(), "Unknown filter: " + name);
        fn = it->second;
    }

    return fn();
}

vector<string>
FilterRegistry::
listFilters()
{
    vector<string> filtersStr;
    auto filters = getFilterRegister();
    {
        lock_guard<Spinlock> guard(filters->filterRegisterLock);

        filtersStr.reserve(filters->filterRegister.size());
        for (const auto& filter : filters->filterRegister)
            filtersStr.push_back(filter.first);
    }

    return filtersStr;
}

} // namepsace RTBKIT
