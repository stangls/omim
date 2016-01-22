#pragma once

#include "base/followed_polyline.hpp"
#include "routing_settings.hpp"
#include "turns.hpp"

#include "geometry/polyline2d.hpp"

#include "std/vector.hpp"
#include "std/set.hpp"
#include "std/string.hpp"

#include "base/logging.hpp"

namespace location
{
  class GpsInfo;
  class RouteMatchingInfo;
}

namespace routing
{

class Route
{
public:
  typedef vector<turns::TurnItem> TTurns;
  typedef pair<uint32_t, double> TTimeItem;
  typedef vector<TTimeItem> TTimes;

  explicit Route(string const & router)
    : m_router(router), m_routingSettings(GetCarRoutingSettings()) {}

  template <class TIter>
  Route(string const & router, TIter beg, TIter end)
    : m_router(router), m_routingSettings(GetCarRoutingSettings()), m_poly(beg, end),
      m_nonFastForward(), m_tourStart(DBL_MAX)
  {
    Update();
  }

  Route(string const & router, vector<m2::PointD> const & points, string const & name = string());

  void Swap(Route & rhs);

  template <class TIter> void SetGeometry(TIter beg, TIter end)
  {
    FollowedPolyline(beg, end).Swap(m_poly);
    Update();
  }

  /**
   * Appends an existing geometry given via start- and end-iterator.
   * Replaces the current FollowedPolyline.
   * @param fastForward if set to true, the Route::MoveIterator() method will not try to fast-forward into this geometry,
   *        i.e. if the current position does match an earlier (not yet visited point in other geometries), it will use that one.
   * @return index of first point in overall geometry
   */
  template <class TIter> size_t AppendGeometry( TIter beg, TIter end, bool fastForward )
  {
      vector<m2::PointD> vector = m_poly.GetPolyline().GetPoints();
      size_t start=vector.size();
      vector.insert(vector.end(),beg,end);
      if (!fastForward){
          // mark via non-fast-forward-interval
          size_t end = vector.size();
          if (end != start){
            m_nonFastForward.emplace_back( start, end );
          }
      }
      FollowedPolyline(vector.begin(),vector.end()).Swap(m_poly);
      Update();
      return start;
  }

  inline void SetTurnInstructions(TTurns & v)
  {
    swap(m_turns, v);
  }
  void AppendTurns(vector<turns::TurnItem>::iterator beg, vector<turns::TurnItem>::iterator end, uint32_t index_offset , uint32_t index_start);

  inline void SetSectionTimes(TTimes & v)
  {
    swap(m_times, v);
  }
  void AppendTimes( vector<double>::iterator beg, vector<double>::iterator end );

  uint32_t GetTotalTimeSec() const;
  uint32_t GetCurrentTimeToEndSec() const;

  FollowedPolyline const & GetFollowedPolyline() const { return m_poly; }

  string const & GetRouterId() const { return m_router; }
  m2::PolylineD const & GetPoly() const { return m_poly.GetPolyline(); }
  TTurns const & GetTurns() const { return m_turns; }
  void GetTurnsDistances(vector<double> & distances) const;
  string const & GetName() const { return m_name; }
  bool IsValid() const { return (m_poly.GetPolyline().GetSize() > 1); }

  double GetTotalDistanceMeters() const;
  double GetCurrentDistanceFromBeginMeters() const;
  double GetCurrentDistanceToEndMeters() const;

  /// \brief GetCurrentTurn returns information about the nearest turn.
  /// \param distanceToTurnMeters is a distance from current position to the nearest turn.
  /// \param turn is information about the nearest turn.
  bool GetCurrentTurn(double & distanceToTurnMeters, turns::TurnItem & turn) const;

  /// @return true if GetNextTurn() returns a valid result in parameters, false otherwise.
  /// \param distanceToTurnMeters is a distance from current position to the second turn.
  /// \param turn is information about the second turn.
  /// @return true if its parameters are filled with correct result.
  /// \note All parameters are filled while a GetNextTurn function call.
  bool GetNextTurn(double & distanceToTurnMeters, turns::TurnItem & turn) const;
  /// \brief Extract information about zero, one or two nearest turns depending on current position.
  /// @return true if its parameter is filled with correct result. (At least with one element.)
  bool GetNextTurns(vector<turns::TurnItemDist> & turns) const;

  void GetCurrentDirectionPoint(m2::PointD & pt) const;

  /**
   * @return true  If position was updated successfully (projection within gps error radius).
   *               May return false if a projection would only be possible into a non-fast-forward geometry.
   *               See Route:AppendGeometry for more information.
   **/
  bool MoveIterator(location::GpsInfo const & info) const;

  void MatchLocationToRoute(location::GpsInfo & location, location::RouteMatchingInfo & routeMatchingInfo) const;

  bool IsCurrentOnEnd() const;

  /// Add country name if we have no country filename to make route
  void AddAbsentCountry(string const & name);

  /// Get absent file list of a routing files for shortest path finding
  set<string> const & GetAbsentCountries() const { return m_absentCountries; }

  inline void SetRoutingSettings(RoutingSettings const & routingSettings)
  {
    m_routingSettings = routingSettings;
    Update();
  }

  void SetTourStart()
  {
      m_tourStart = m_poly.GetPolyline().GetLength();
  }
  double GetTourStart() const {
      return m_tourStart;
  }

private:
  /// Call this fucnction when geometry have changed.
  void Update();
  double GetPolySegAngle(size_t ind) const;
  TTurns::const_iterator GetCurrentTurn() const;

private:
  friend string DebugPrint(Route const & r);

  string m_router;
  RoutingSettings m_routingSettings;
  string m_name;

  FollowedPolyline m_poly;
  FollowedPolyline m_simplifiedPoly;

  set<string> m_absentCountries;

  TTurns m_turns;
  TTimes m_times;

  mutable double m_currentTime;

  GeometryIntervals m_nonFastForward;

  double m_tourStart;
};

} // namespace routing
