#ifndef TOURROUTER_H
#define TOURROUTER_H

#include "router.hpp"

#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"

namespace routing
{

class TourRouter : public IRouter
{
public:
    TourRouter(unique_ptr<IRouter> router );
    ~TourRouter();
private:
    shared_ptr<IRouter> m_router;
};

}  // end namespace routing

#endif // TOURROUTER_H
