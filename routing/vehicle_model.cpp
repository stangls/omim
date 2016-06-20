#include "routing/vehicle_model.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/macros.hpp"

#include "std/limits.hpp"
#include "std/initializer_list.hpp"

namespace routing
{

VehicleModel::VehicleModel(Classificator const & c, InitListT const & speedLimits)
  : m_maxSpeedKMpH(0),
    m_onewayType(c.GetTypeByPath({ "hwtag", "oneway" }))
{
  for (auto const & v : speedLimits)
  {
    m_maxSpeedKMpH = max(m_maxSpeedKMpH, v.m_speedKMpH);
    m_types[c.GetTypeByPath(vector<string>(v.m_types, v.m_types + 2))] = v.m_speedKMpH;
  }
}

void VehicleModel::SetAdditionalRoadTypes(Classificator const & c,
                                          initializer_list<char const *> const * arr, size_t sz)
{
  for (size_t i = 0; i < sz; ++i)
    m_addRoadTypes.push_back(c.GetTypeByPath(arr[i]));
}

double VehicleModel::GetSpeed(FeatureType const & f) const
{
  feature::TypesHolder const types(f);

  RoadAvailability const restriction = GetRoadAvailability(types);
  if (restriction == RoadAvailability::Available)
    return GetMaxSpeed();
  if (restriction != RoadAvailability::NotAvailable && HasRoadType(types))
    return GetMinTypeSpeed(types);

  return 0.0;
}

double VehicleModel::GetMinTypeSpeed(feature::TypesHolder const & types) const
{
  double speed = m_maxSpeedKMpH * 2;
  for (uint32_t t : types)
  {
    uint32_t const type = ftypes::BaseChecker::PrepareToMatch(t, 2);
    auto it = m_types.find(type);
    if (it != m_types.end())
      speed = min(speed, it->second);
  }
  if (speed <= m_maxSpeedKMpH)
    return speed;

  return 0.0;
}

bool VehicleModel::IsOneWay(FeatureType const & f) const
{
  return HasOneWayType(feature::TypesHolder(f));
}

bool VehicleModel::HasOneWayType(feature::TypesHolder const & types) const
{
  return types.Has(m_onewayType);
}

bool VehicleModel::IsRoad(FeatureType const & f) const
{
  if (f.GetFeatureType() != feature::GEOM_LINE)
    return false;

  feature::TypesHolder const types(f);

  if (GetRoadAvailability(types) == RoadAvailability::NotAvailable)
    return false;
  return HasRoadType(types);
}

bool VehicleModel::IsRoadType(uint32_t type) const
{
  return find(m_addRoadTypes.begin(), m_addRoadTypes.end(), type) != m_addRoadTypes.end() ||
         m_types.find(ftypes::BaseChecker::PrepareToMatch(type, 2)) != m_types.end();
}

IVehicleModel::RoadAvailability VehicleModel::GetRoadAvailability(feature::TypesHolder const & /* types */) const
{
  return RoadAvailability::Unknown;
}

string DebugPrint(IVehicleModel::RoadAvailability const l)
{
  switch (l)
  {
  case IVehicleModel::RoadAvailability::Available: return "Available";
  case IVehicleModel::RoadAvailability::NotAvailable: return "NotAvailable";
  case IVehicleModel::RoadAvailability::Unknown: return "Unknown";
  }

  stringstream out;
  out << "Unknown IVehicleModel::RoadAvailability (" << static_cast<int>(l) << ")";
  return out.str();
}
}  // namespace routing
