#include "tour_router.h"

namespace routing
{

TourRouter::TourRouter(  unique_ptr<IRouter> router )
{
  m_router = move(router);
}

TourRouter::~TourRouter()
{
  if (m_router)
  {
    m_router.reset();
  }
}

} // end namespace routing

