#include "followed_polyline.hpp"
#include "geometry/angles.hpp"
#include "base/logging.hpp"

namespace routing
{

using Iter = routing::FollowedPolyline::Iter;

Iter FollowedPolyline::Begin() const
{
  ASSERT(IsValid(), ());
  return Iter(m_poly.Front(), 0);
}

Iter FollowedPolyline::End() const
{
  ASSERT(IsValid(), ());
  return Iter(m_poly.Back(), m_poly.GetSize() - 1);
}

Iter FollowedPolyline::GetIterToIndex(size_t index) const
{
  ASSERT(IsValid(), ());
  ASSERT_LESS(index, m_poly.GetSize(), ());

  return Iter(m_poly.GetPoint(index), index);
}

double FollowedPolyline::GetDistanceM(Iter const & it1, Iter const & it2) const
{
  ASSERT(IsValid(), ());
  ASSERT(it1.IsValid() && it2.IsValid(), ());
  ASSERT_LESS_OR_EQUAL(it1.m_ind, it2.m_ind, ());
  ASSERT_LESS(it1.m_ind, m_poly.GetSize(), ());
  ASSERT_LESS(it2.m_ind, m_poly.GetSize(), ());

  if (it1.m_ind == it2.m_ind)
    return MercatorBounds::DistanceOnEarth(it1.m_pt, it2.m_pt);

  return (MercatorBounds::DistanceOnEarth(it1.m_pt, m_poly.GetPoint(it1.m_ind + 1)) +
          m_segDistance[it2.m_ind - 1] - m_segDistance[it1.m_ind] +
          MercatorBounds::DistanceOnEarth(m_poly.GetPoint(it2.m_ind), it2.m_pt));
}

double FollowedPolyline::GetTotalDistanceM() const
{
  ASSERT(IsValid(), ());
  return m_segDistance.back();
}

double FollowedPolyline::GetDistanceFromBeginM() const
{
  ASSERT(IsValid(), ());
  ASSERT(m_current.IsValid(), ());

  return (m_current.m_ind > 0 ? m_segDistance[m_current.m_ind - 1] : 0.0) +
         MercatorBounds::DistanceOnEarth(m_current.m_pt, m_poly.GetPoint(m_current.m_ind));
}

double FollowedPolyline::GetDistanceToEndM() const
{
  return GetTotalDistanceM() - GetDistanceFromBeginM();
}

void FollowedPolyline::Swap(FollowedPolyline & rhs)
{
  m_poly.Swap(rhs.m_poly);
  m_segDistance.swap(rhs.m_segDistance);
  m_segProj.swap(rhs.m_segProj);
  swap(m_current, rhs.m_current);
  swap(m_lastNonCrossingDistance, rhs.m_lastNonCrossingDistance);
}

void FollowedPolyline::Update()
{
  size_t n = m_poly.GetSize();
  ASSERT_GREATER(n, 1, ());
  --n;

  m_segDistance.resize(n);
  m_segProj.resize(n);

  double dist = 0.0;
  for (size_t i = 0; i < n; ++i)
  {
    m2::PointD const & p1 = m_poly.GetPoint(i);
    m2::PointD const & p2 = m_poly.GetPoint(i + 1);

    dist += MercatorBounds::DistanceOnEarth(p1, p2);

    m_segDistance[i] = dist;
    m_segProj[i].SetBounds(p1, p2);
  }

  m_current = Iter(m_poly.Front(), 0);
  UpdateLastNonCrossing();

}

template <class DistanceFn>
Iter FollowedPolyline::GetClosestProjection(m2::RectD const & posRect, const vector<TInterval> &nonFastForward,
                                            DistanceFn const & distFn) const
{
  //LOG(my::LINFO,("GetClosestProjection"));
  Iter res;
  double minDist = numeric_limits<double>::max();

  m2::PointD const currPos = posRect.Center();
  size_t const count = m_poly.GetSize() - 1;
  int noCheckFastForward=2; // for two iterations do not check fast-forward ( = current position and next position)
  for (size_t i = m_current.m_ind; i < count; ++i)
  {
    if ( !noCheckFastForward ) {
        // skip if we do not fast-forward into this interval
        int skipTo=0;
        for ( const TInterval &nonFF : nonFastForward ) {
            int start, end;
            std::tie (start, end) = nonFF;
            if ( i>=start && i<end ){
                //LOG(my::LINFO,("? skip",start,"<=",i,"<",end));
                ASSERT( start<end, ("internal error with ordering of non-ff-intervals") );
                skipTo=end;
                break;
            }
        }
        if (skipTo) {
            //LOG(my::LINFO,("skipTo=",skipTo));
            ASSERT( i<=skipTo-1, ("internal error with non-ff-intervals") );
            i=skipTo-1; // will be incremented on continuing for-loop
            //LOG(my::LINFO,("skip to",i,count));
            continue;
        }
    }else
        noCheckFastForward--;

    m2::PointD const pt = m_segProj[i](currPos);

    if (!posRect.IsPointInside(pt))
      continue;

    Iter it(pt, i);
    double const dp = distFn(it);
    if (dp < minDist)
    {
      res = it;
      minDist = dp;
    }
  }
  //LOG(my::LINFO,("GetClosestProjection done"));
  return res;
}

Iter FollowedPolyline::UpdateProjectionByPrediction(
        m2::RectD const & posRect,
        double predictDistance,
        const vector<TInterval> & nonFastForward
) const {
  ASSERT(m_current.IsValid(), ());
  ASSERT_LESS(m_current.m_ind, m_poly.GetSize() - 1, ());

  if (predictDistance <= 0.0)
    return UpdateProjection(posRect, nonFastForward);

  Iter res;
  res = GetClosestProjection(posRect, nonFastForward, [&](Iter const & it)
  {
    return fabs(GetDistanceM(m_current, it) - predictDistance);
  });

  if (res.IsValid()){
    m_current = res;
    UpdateLastNonCrossing();
  }
  return res;
}

Iter FollowedPolyline::UpdateProjection(m2::RectD const & posRect, const vector<TInterval> & nonFastForward) const
{
  ASSERT(m_current.IsValid(), ());
  ASSERT_LESS(m_current.m_ind, m_poly.GetSize() - 1, ());

  Iter res;
  m2::PointD const currPos = posRect.Center();
  res = GetClosestProjection(posRect, nonFastForward, [&](Iter const & it)
  {
    return MercatorBounds::DistanceOnEarth(it.m_pt, currPos);
  });

  if (res.IsValid()){
    m_current = res;
    UpdateLastNonCrossing();
  }
  return res;
}

double FollowedPolyline::GetMercatorDistanceFromBegin() const
{
  double distance = 0.0;
  if (m_current.IsValid())
  {
    for (size_t i = 1; i <= m_current.m_ind; i++)
      distance += m_poly.GetPoint(i).Length(m_poly.GetPoint(i - 1));

    distance += m_poly.GetPoint(m_current.m_ind).Length(m_current.m_pt);
  }

  return distance;
}

void FollowedPolyline::GetCurrentDirectionPoint(m2::PointD & pt, double toleranceM) const
{
  ASSERT(IsValid(), ());
  size_t currentIndex = min(m_current.m_ind + 1, m_poly.GetSize() - 1);
  m2::PointD point = m_poly.GetPoint(currentIndex);
  for (; currentIndex < m_poly.GetSize() - 1; point = m_poly.GetPoint(++currentIndex))
  {
    if (MercatorBounds::DistanceOnEarth(point, m_current.m_pt) > toleranceM)
      break;
  }

  pt = point;
}

void FollowedPolyline::UpdateLastNonCrossing() const
{
    if (m_current.IsValid()) {
        uint max=1000; // total upper computation time-limit
        double lastNonCrossingDistance = 0.0;
        m2::PointD point0 = m_current.m_pt;
        size_t i = m_current.m_ind+1;
        m2::PointD point1 = m_poly.GetPoint(i);
        double angle = ang::AngleTo(point0,point1);
        double cMin=-DBL_MAX;
        double cMax=+DBL_MAX;
        for (; i<m_poly.GetSize(); i++){
            point1 = m_poly.GetPoint(i);

            double len = point0.Length(point1);
            if (len>0.000000001) {
                double angle = ang::AngleTo(point0,point1);
                if (angle<cMin)
                    angle+=2*M_PI;
                else if (angle>cMax)
                    angle-=2*M_PI;
                if (angle>cMin && angle<cMax){
                    cMin=fmax(cMin,angle-M_PI);
                    cMax=fmin(cMax,angle+M_PI);
                } else
                    break;
            }

            lastNonCrossingDistance += len;
            if ( !(--max) ) // max==0?
                break;
            point0 = point1;
        }
        m_lastNonCrossingDistance=lastNonCrossingDistance;
    }else{
        // we do not know of any last point which does not cross. assume it does not exist so the route is drawn fully (this is better than draw the whole route non-fully)
        m_lastNonCrossingDistance=DBL_MAX;
    }
}

double FollowedPolyline::GetCurrentNonCrossingDistance() const
{
    return m_lastNonCrossingDistance;
}

}  //  namespace routing
