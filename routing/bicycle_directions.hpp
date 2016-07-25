#pragma once

#include "routing/directions_engine.hpp"
#include "routing/loaded_path_segment.hpp"
#include "routing/turn_candidate.hpp"

#include "std/map.hpp"
#include "std/unique_ptr.hpp"

class Index;

namespace routing
{
class BicycleDirectionsEngine : public IDirectionsEngine
{
public:
  struct AdjacentEdges
  {
    explicit AdjacentEdges(size_t ingoingTurnsCount = 0) : m_ingoingTurnsCount(ingoingTurnsCount) {}

    turns::TurnCandidates m_outgoingTurns;
    size_t m_ingoingTurnsCount;
  };

  using TAdjacentEdgesMap = map<TNodeId, AdjacentEdges>;

  BicycleDirectionsEngine(Index const & index);

  // IDirectionsEngine override:
  void Generate(IRoadGraph const & graph, vector<Junction> const & path, Route::TTimes & times,
                Route::TTurns & turns, vector<m2::PointD> & routeGeometry,
                my::Cancellable const & cancellable) override;

private:
  Index::FeaturesLoaderGuard & GetLoader(MwmSet::MwmId const & id);
  void LoadPathGeometry(FeatureID const & featureId, vector<m2::PointD> const & path,
                        LoadedPathSegment & pathSegment);

  TAdjacentEdgesMap m_adjacentEdges;
  TUnpackedPathSegments m_pathSegments;
  Index const & m_index;
  unique_ptr<Index::FeaturesLoaderGuard> m_loader;
};
}  // namespace routing
