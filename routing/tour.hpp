#ifndef TOURROUTER_H
#define TOURROUTER_H

#include "router.hpp"

#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"

#include <vector>
#include <iterator>

namespace routing
{

typedef m2::PointD PointD;
typedef vector<PointD> pvec;

class Tour
{
public:
    Tour();
    Tour( string const & filePath );
    ~Tour();
    string GetName();

    bool UpdateCurrentPosition(size_t index);
    vector<PointD> &GetAllPoints();

    pvec::iterator GetCurrentIt() {
        return m_points.begin()+m_currentIndex;
    }
    pvec::iterator GetEndIt() {
        return m_points.end();
    }

private:
    size_t m_currentIndex;
    pvec m_points;
};

}  // end namespace routing

#endif // TOURROUTER_H
