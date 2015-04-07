#include "astar_router.hpp"

#include "../base/logging.hpp"
#include "../base/timer.hpp"
#include "../geometry/distance_on_sphere.hpp"
#include "../indexer/mercator.hpp"

namespace routing
{

static double const MAX_SPEED = 36.11111111111; // m/s

void AStarRouter::SetFinalRoadPos(vector<RoadPos> const & finalPos)
{
  LOG(LINFO, ("AStarRouter::SetFinalRoadPos() call"));
  for (auto const & roadPos : finalPos)
    LOG(LINFO, ("AStarRouter::SetFinalRoadPos(): finalPos:", DebugPrint(roadPos)));

  ASSERT_GREATER(finalPos.size(), 0, ());
  m_entries.clear();
  PossiblePathQueueT().swap(m_queue);
  for (size_t i = 0; i < finalPos.size(); ++i)
  {
    pair<ShortPathSetT::iterator, bool> t = m_entries.insert(ShortestPath(finalPos[i]));
    ASSERT(t.second, ());
    m_queue.push(PossiblePath(&*t.first));
  }

}

void AStarRouter::CalculateRoute(vector<RoadPos> const & startPos, vector<RoadPos> & route)
{
  my::Timer timer;
  LOG(LINFO, ("AStarRouter::CalculateRoute() call start"));
  for (auto const & roadPos : startPos)
    LOG(LINFO, ("AStarRouter::CalculateRoute(): startPos:", DebugPrint(roadPos)));

  route.clear();
  set<RoadPos> startSet(startPos.begin(), startPos.end());
  set<uint32_t> startFeatureSet;
  for (vector<RoadPos>::const_iterator it = startPos.begin(); it != startPos.end(); ++it)
    startFeatureSet.insert(it->GetFeatureId());

  while (!m_queue.empty())
  {
    PossiblePath const & currPath = m_queue.top();
    double const fScore = currPath.m_fScore;
    ShortestPath const * current = currPath.m_path;
    current->RemovedFromOpenSet();
    m_queue.pop();

    current->SetVisited();

    bool const bStartFeature = startFeatureSet.find(current->GetPos().GetFeatureId()) != startFeatureSet.end();
    if (bStartFeature && startSet.find(current->GetPos()) != startSet.end())
    {
      ReconstructRoute(current, route);
      LOG(LINFO, ("AStarRouter::CalculateRoute() call finish: elapsed:", timer.ElapsedSeconds()));
      return;
    }

    IRoadGraph::TurnsVectorT turns;
    m_pRoadGraph->GetPossibleTurns(current->GetPos(), turns, bStartFeature);
    for (IRoadGraph::TurnsVectorT::const_iterator it = turns.begin(); it != turns.end(); ++it)
    {
      PossibleTurn const & turn = *it;
      pair<ShortPathSetT::iterator, bool> t = m_entries.insert(ShortestPath(turn.m_pos, current));
      if (t.second || !t.first->IsVisited())
      {
        ShortestPath const * nextPath = &*t.first;
        ASSERT_GREATER(turn.m_speed, 0.0, ());
        double nextScoreG = fScore + turn.m_secondsCovered;//DistanceBetween(current, nextPath) / turn.m_speed;

        if (!nextPath->IsInOpenSet() || nextScoreG < nextPath->GetScoreG())
        {
          nextPath->SetParent(current);
          nextPath->SetScoreG(nextScoreG);
          if (!nextPath->IsInOpenSet())
          {
            m_queue.push(PossiblePath(nextPath, nextScoreG + HeuristicCostEstimate(nextPath, startSet)));
            nextPath->AppendedIntoOpenSet();
          }
        }
      }
    }
  }

  LOG(LDEBUG, ("No route found!"));
  LOG(LINFO, ("AStarRouter::CalculateRoute() call finish: elapsed:", timer.ElapsedSeconds()));
  // Route not found.
}

double AStarRouter::HeuristicCostEstimate(AStarRouter::ShortestPath const * s1, set<RoadPos> const & goals)
{
  /// @todo support of more than one goals
  ASSERT_GREATER(goals.size(), 0, ());

  m2::PointD const & b = (*goals.begin()).GetSegEndpoint();
  m2::PointD const & e = s1->GetPos().GetSegEndpoint();

  return ms::DistanceOnEarth(MercatorBounds::YToLat(b.y),
                             MercatorBounds::XToLon(b.x),
                             MercatorBounds::YToLat(e.y),
                             MercatorBounds::XToLon(e.x)) / MAX_SPEED;
}

double AStarRouter::DistanceBetween(ShortestPath const * p1, ShortestPath const * p2)
{
  m2::PointD const & b = p1->GetPos().GetSegEndpoint();
  m2::PointD const & e = p2->GetPos().GetSegEndpoint();
  return ms::DistanceOnEarth(MercatorBounds::YToLat(b.y),
                             MercatorBounds::XToLon(b.x),
                             MercatorBounds::YToLat(e.y),
                             MercatorBounds::XToLon(e.x));
}

void AStarRouter::ReconstructRoute(ShortestPath const * path, vector<RoadPos> & route) const
{
  ShortestPath const * p = path;

  while (p)
  {
    route.push_back(p->GetPos());
    if (p->GetParentEntry())
      p = p->GetParentEntry();
    else
      p = 0;
  }
}

}