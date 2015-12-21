#ifndef TOURROUTER_H
#define TOURROUTER_H

#include "router.hpp"

#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"

namespace routing
{

typedef m2::PointD PointD;

class Tour
{
public:
    Tour();
    ~Tour();
    PointD GetCurrentPoint();
    bool UpdateCurrentPosition(size_t index);
    vector<PointD> &GetAllPoints();
    string GetName();
private:
    size_t m_currentIndex;
    vector<m2::PointD> m_points;
};

}  // end namespace routing

#endif // TOURROUTER_H
