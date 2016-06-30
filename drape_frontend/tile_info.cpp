#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/rule_drawer.hpp"
#include "drape_frontend/stylist.hpp"
#include "drape_frontend/tile_info.hpp"

#include "indexer/scales.hpp"

#include "base/scope_guard.hpp"
#include "base/logging.hpp"

#include "std/bind.hpp"

namespace df
{

TileInfo::TileInfo(drape_ptr<EngineContext> && context)
  : m_context(move(context))
  , m_is3dBuildings(false)
  , m_isCanceled(false)
{
}

m2::RectD TileInfo::GetGlobalRect() const
{
  return GetTileKey().GetGlobalRect();
}

void TileInfo::ReadFeatureIndex(MapDataProvider const & model)
{
  m2::RectD rect = GetGlobalRect();
/*
  LOG(my::LDEBUG,("TileInfo::ReadFeatureIndex for",
    MercatorBounds::XToLon(rect.LeftTop().x),
    MercatorBounds::YToLat(rect.LeftTop().y),
    MercatorBounds::XToLon(rect.RightBottom().x),
    MercatorBounds::YToLat(rect.RightBottom().y)
  ));*/


  if (DoNeedReadIndex())
  {
    CheckCanceled();

    size_t const kAverageFeaturesCount = 256;
    m_featureInfo.reserve(kAverageFeaturesCount);

#ifdef DEBUG
    set<MwmSet::MwmId> existing;
    MwmSet::MwmId lastMwm;
    model.ReadFeaturesID([this, &existing, &lastMwm](FeatureID const & id)
    {
      if (existing.empty() || lastMwm != id.m_mwmId)
      {
        ASSERT(existing.insert(id.m_mwmId).second, ());
        lastMwm = id.m_mwmId;
      }
#else
    model.ReadFeaturesID([this](FeatureID const & id)
    {
#endif
      m_featureInfo.push_back(id);
    }, GetGlobalRect(), GetZoomLevel());
  }

  vector<shared_ptr<CustomGeom>> temp(CustomGeometries::GetInstance()->GetGeometries(rect));
  m_customGeoms = temp;

}

void TileInfo::ReadFeatures(MapDataProvider const & model)
{
  m_context->BeginReadTile();

  // Reading can be interrupted by exception throwing
  MY_SCOPE_GUARD(ReleaseReadTile, bind(&EngineContext::EndReadTile, m_context.get()));

  ReadFeatureIndex(model);
  CheckCanceled();

  m2::RectD rect = GetGlobalRect();
  /*LOG(my::LDEBUG,("TileInfo::ReadFeatures for",
    MercatorBounds::XToLon(rect.LeftTop().x),
    MercatorBounds::YToLat(rect.LeftTop().y),
    MercatorBounds::XToLon(rect.RightBottom().x),
    MercatorBounds::YToLat(rect.RightBottom().y)
  ));*/

  if (!m_featureInfo.empty() || !m_customGeoms.empty())
  {
    RuleDrawer drawer(bind(&TileInfo::InitStylist, this, _1, _2),
                      bind(&TileInfo::IsCancelled, this),
                      model.m_isCountryLoadedByName,
                      make_ref(m_context), m_is3dBuildings);
    if (!m_featureInfo.empty()){
        model.ReadFeatures(bind<void>(ref(drawer), _1), m_featureInfo);
    }

    for (vector<shared_ptr<CustomGeom>>::const_iterator it = m_customGeoms.cbegin(); it!=m_customGeoms.cend(); it++){
        drawer.AddCustomGeometry(*it);
    }
  }

}

void TileInfo::Cancel()
{
  m_isCanceled = true;
}

bool TileInfo::IsCancelled() const
{
  return m_isCanceled;
}

void TileInfo::InitStylist(FeatureType const & f, Stylist & s)
{
  CheckCanceled();
  df::InitStylist(f, m_context->GetTileKey().m_zoomLevel, m_is3dBuildings, s);
}

bool TileInfo::DoNeedReadIndex() const
{
  return m_featureInfo.empty();
}

void TileInfo::CheckCanceled() const
{
  if (m_isCanceled)
    MYTHROW(ReadCanceledException, ());
}

int TileInfo::GetZoomLevel() const
{
  return ClipTileZoomByMaxDataZoom(m_context->GetTileKey().m_zoomLevel);
}

} // namespace df
