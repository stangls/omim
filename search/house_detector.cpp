#include "house_detector.hpp"
#include "search_common.hpp"
#include "algos.hpp"

#include "../indexer/feature_impl.hpp"
#include "../indexer/classificator.hpp"

#include "../geometry/distance.hpp"
#include "../geometry/angles.hpp"

#include "../base/logging.hpp"
#include "../base/stl_iterator.hpp"
#include "../base/limited_priority_queue.hpp"

#include "../std/set.hpp"
#include "../std/bind.hpp"
#include "../std/numeric.hpp"

#ifdef DEBUG
#include "../platform/platform.hpp"

#include "../std/iostream.hpp"
#include "../std/fstream.hpp"
#endif


namespace search
{

namespace
{

#if 0
void Houses2KML(ostream & s, map<search::House, double> const & m)
{
  for (map<search::House, double>::const_iterator it = m.begin(); it != m.end(); ++it)
  {
    m2::PointD const & pt = it->first.GetPosition();

    s << "<Placemark>"
      << "<name>" << it->first.GetNumber() <<  "</name>"

      << "<Point><coordinates>"
            << MercatorBounds::XToLon(pt.x)
            << ","
            << MercatorBounds::YToLat(pt.y)

      << "</coordinates></Point>"
      << "</Placemark>" << endl;
  }
}

void Street2KML(ostream & s, vector<m2::PointD> const & pts, char const * color)
{
  s << "<Placemark>" << endl;
  s << "<Style><LineStyle><color>" << color << "</color></LineStyle></Style>" << endl;

  s << "<LineString><coordinates>" << endl;
  for (size_t i = 0; i < pts.size(); ++i)
  {
    s << MercatorBounds::XToLon(pts[i].x) << "," << MercatorBounds::YToLat(pts[i].y) << "," << "0.0" << endl;
  }
  s << "</coordinates></LineString>" << endl;

  s << "</Placemark>" << endl;
}

void Streets2KML(ostream & s, MergedStreet const & st, char const * color)
{
  for (size_t i = 0; i < st.m_cont.size(); ++i)
    Street2KML(s, st.m_cont[i]->m_points, color);
}

class KMLFileGuard
{
  ofstream m_file;
public:
  KMLFileGuard(string const & name)
  {
    m_file.open(GetPlatform().WritablePathForFile(name).c_str());

    m_file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl;
    m_file << "<kml xmlns=\"http://earth.google.com/kml/2.2\">" << endl;
    m_file << "<Document>" << endl;
  }

  ostream & GetStream() { return m_file; }

  ~KMLFileGuard()
  {
    m_file << "</Document></kml>" << endl;
  }
};
#endif


double const STREET_CONNECTION_LENGTH_M = 100.0;
int const HN_NEARBY_DISTANCE = 4;
double const STREET_CONNECTION_MAX_ANGLE = math::pi / 2.0;
size_t const HN_COUNT_FOR_ODD_TEST = 16;
//double const HN_MIN_READ_OFFSET_M = 50.0;
//int const HN_NEARBY_INDEX_RANGE = 5;
double const HN_MAX_CONNECTION_DIST_M = 300.0;


class StreetCreator
{
  Street * m_street;
public:
  StreetCreator(Street * st) : m_street(st) {}
  void operator () (m2::PointD const & pt) const
  {
    m_street->m_points.push_back(pt);
  }
};

}

ParsedNumber::ParsedNumber(string const & number, bool american)
  : m_fullN(number)
{
  strings::MakeLowerCase(m_fullN);

  char * curr;
  m_startN = strtol(number.c_str(), &curr, 10);
  m_endN = -1;
  ASSERT_GREATER_OR_EQUAL(m_startN, 0, (number));

  bool hasMinus = false;
  bool hasComma = false;
  while (curr && *curr != 0)
  {
    switch (*curr)
    {
    case ' ': case '\t': ++curr; break;
    case ',': case ';': ++curr; hasComma = true; break;
    case '-': ++curr; hasMinus = true; break;
    default:
      {
        if (hasComma || hasMinus)
        {
          char const * start = curr;
          long const x = strtol(start, &curr, 10);
          if (curr != start)
          {
            m_endN = x;
            ASSERT_GREATER_OR_EQUAL(m_endN, 0, (number));
            break;
          }
        }

        curr = 0;
        break;
      }
    }
  }

  if (m_endN != -1)
  {
    if (hasMinus && american)
    {
      m_startN = m_startN * 100 + m_endN;
      m_endN = -1;
    }
    else
    {
      if (abs(m_endN - m_startN) >= 2*HN_NEARBY_DISTANCE)
        m_endN = -1;
      else
      {
        if (m_startN > m_endN)
          std::swap(m_startN, m_endN);
      }
    }
  }
}

bool ParsedNumber::IsIntersect(ParsedNumber const & number, int offset) const
{
  int const n = number.GetIntNumber();
  if (((m_endN == -1) && abs(GetIntNumber() - n) > offset) ||
      ((m_endN != -1) && (m_startN - offset > n || m_endN + offset < n)))
  {
    return false;
  }
  return true;
}

int House::GetMatch(ParsedNumber const & number) const
{
  if (!m_number.IsIntersect(number))
    return -1;

  if (m_number.GetNumber() == number.GetNumber())
    return 0;

  if (m_number.IsOdd() == number.IsOdd())
    return 1;

  return 2;
}

bool House::GetNearbyMatch(ParsedNumber const & number) const
{
  return m_number.IsIntersect(number, HN_NEARBY_DISTANCE);
}

FeatureLoader::FeatureLoader(Index const * pIndex)
  : m_pIndex(pIndex), m_pGuard(0)
{
}

FeatureLoader::~FeatureLoader()
{
  Free();
}

void FeatureLoader::CreateLoader(size_t mwmID)
{
  if (m_pGuard == 0 || mwmID != m_pGuard->GetID())
  {
    delete m_pGuard;
    m_pGuard = new Index::FeaturesLoaderGuard(*m_pIndex, mwmID);
  }
}

void FeatureLoader::Load(FeatureID const & id, FeatureType & f)
{
  CreateLoader(id.m_mwm);
  m_pGuard->GetFeature(id.m_offset, f);
}

void FeatureLoader::Free()
{
  delete m_pGuard;
  m_pGuard = 0;
}

template <class ToDo>
void FeatureLoader::ForEachInRect(m2::RectD const & rect, ToDo toDo)
{
  m_pIndex->ForEachInRect(toDo, rect, scales::GetUpperScale());
}

m2::RectD Street::GetLimitRect(double offsetMeters) const
{
  m2::RectD rect;
  for (size_t i = 0; i < m_points.size(); ++i)
    rect.Add(MercatorBounds::RectByCenterXYAndSizeInMeters(m_points[i], offsetMeters));
  return rect;
}

void Street::SetName(string const & name)
{
  m_name = name;
  GetStreetNameAsKey(name, m_processedName);
}

namespace
{

bool LessStreetDistance(HouseProjection const & p1, HouseProjection const & p2)
{
  return p1.m_streetDistance < p2.m_streetDistance;
}

double GetDistanceMeters(m2::PointD const & p1, m2::PointD const & p2)
{
  return MercatorBounds::DistanceOnEarth(p1, p2);
}

pair<double, double> GetConnectionAngleAndDistance(bool & isBeg, Street const * s1, Street const * s2)
{
  m2::PointD const & p1 = isBeg ? s1->m_points.front() : s1->m_points.back();
  m2::PointD const & p0 = isBeg ? s1->m_points[1] : s1->m_points[s1->m_points.size()-2];

  double const d0 = p1.SquareLength(s2->m_points.front());
  double const d2 = p1.SquareLength(s2->m_points.back());
  isBeg = (d0 < d2);
  m2::PointD const & p2 = isBeg ? s2->m_points[1] : s2->m_points[s2->m_points.size()-2];

  return make_pair(ang::GetShortestDistance(ang::AngleTo(p0, p1), ang::AngleTo(p1, p2)), min(d0, d2));
}

}

void Street::Reverse()
{
  ASSERT(m_houses.empty(), ());
  reverse(m_points.begin(), m_points.end());
}

void Street::SortHousesProjection()
{
  sort(m_houses.begin(), m_houses.end(), &LessStreetDistance);
}

HouseDetector::HouseDetector(Index const * pIndex)
  : m_loader(pIndex), m_streetNum(0)
{
  // default value for conversions
  SetMetres2Mercator(360.0 / 40.0E06);
}

HouseDetector::~HouseDetector() { ClearCaches(); }

void HouseDetector::SetMetres2Mercator(double factor)
{
  m_metres2Mercator = factor;

  LOG(LDEBUG, ("Street join epsilon = ", m_metres2Mercator * STREET_CONNECTION_LENGTH_M));
}

double HouseDetector::GetApprLengthMeters(int index) const
{
  m2::PointD const & p1 = m_streets[index].m_cont.front()->m_points.front();
  m2::PointD const & p2 = m_streets[index].m_cont.back()->m_points.back();
  return p1.Length(p2) / m_metres2Mercator;
}

HouseDetector::StreetPtr HouseDetector::FindConnection(Street const * st, bool beg) const
{
  m2::PointD const & pt = beg ? st->m_points.front() : st->m_points.back();

  StreetPtr resStreet(0, false);
  double resDistance = numeric_limits<double>::max();
  double const minSqDistance = math::sqr(m_metres2Mercator * STREET_CONNECTION_LENGTH_M);

  for (size_t i = 0; i < m_end2st.size(); ++i)
  {
    if (pt.SquareLength(m_end2st[i].first) > minSqDistance)
      continue;

    Street * current = m_end2st[i].second;

    // Choose the possible connection from non-processed and from the same street parts.
    if (current != st && (current->m_number == -1 || current->m_number == m_streetNum) &&
        Street::IsSameStreets(st, current))
    {
      // Choose the closest connection with suitable angle.
      bool isBeg = beg;
      pair<double, double> const res = GetConnectionAngleAndDistance(isBeg, st, current);
      if (fabs(res.first) < STREET_CONNECTION_MAX_ANGLE && res.second < resDistance)
      {
        resStreet = StreetPtr(current, isBeg);
        resDistance = res.second;
      }
    }
  }

  if (resStreet.first && resStreet.first->m_number == -1)
    return resStreet;
  else
    return StreetPtr(0, false);
}

void HouseDetector::MergeStreets(Street * st)
{
  st->m_number = m_streetNum;

  m_streets.push_back(MergedStreet());
  MergedStreet & ms = m_streets.back();
  ms.m_cont.push_back(st);

  bool isBeg = true;
  while (true)
  {
    // find connection from begin or end
    StreetPtr st(0, false);
    if (isBeg)
      st = FindConnection(ms.m_cont.front(), true);
    if (st.first == 0)
    {
      isBeg = false;
      st = FindConnection(ms.m_cont.back(), false);
      if (st.first == 0)
        return;
    }

    if (isBeg == st.second)
      st.first->Reverse();

    st.first->m_number = m_streetNum;

    if (isBeg)
      ms.m_cont.push_front(st.first);
    else
      ms.m_cont.push_back(st.first);
  }
}

int HouseDetector::LoadStreets(vector<FeatureID> const & ids)
{
  //LOG(LDEBUG, ("IDs = ", ids));

  ASSERT(IsSortedAndUnique(ids.begin(), ids.end()), ());

  // Check if the cache is obsolete and need to be cleared.
  if (!m_id2st.empty())
  {
    typedef pair<FeatureID, Street *> ValueT;
    function<ValueT::first_type const & (ValueT const &)> f = bind(&ValueT::first, _1);

    // Do clear cache if we have elements that are present in the one set,
    // but not in the other one (set's order is irrelevant).
    size_t const count = set_intersection(make_transform_iterator(m_id2st.begin(), f),
                                          make_transform_iterator(m_id2st.end(), f),
                                          ids.begin(), ids.end(),
                                          CounterIterator()).GetCount();

    if (count < min(ids.size(), m_id2st.size()))
    {
      LOG(LDEBUG, ("Clear HouseDetector cache: "
                   "Common =", count, "Cache =", m_id2st.size(), "Input =", ids.size()));
      ClearCaches();
    }
    else if (m_id2st.size() > ids.size() * 1.2)
    {
      LOG(LDEBUG, ("Clear unused"));
      ClearUnusedStreets(ids);
    }
  }

  // Load streets.
  int count = 0;
  for (size_t i = 0; i < ids.size(); ++i)
  {
    if (m_id2st.find(ids[i]) != m_id2st.end())
      continue;

    FeatureType f;
    m_loader.Load(ids[i], f);
    if (f.GetFeatureType() == feature::GEOM_LINE)
    {
      // Use default name as a primary compare key for merging.
      string name;
      if (!f.GetName(FeatureType::DEFAULT_LANG, name))
        continue;
      ASSERT(!name.empty(), ());

      ++count;

      Street * st = new Street();
      st->SetName(name);
      f.ForEachPoint(StreetCreator(st), FeatureType::BEST_GEOMETRY);

      if (m_end2st.empty())
      {
        m2::PointD const p1 = st->m_points.front();
        m2::PointD const p2 = st->m_points.back();

        SetMetres2Mercator(p1.Length(p2) / GetDistanceMeters(p1, p2));
      }

      m_id2st[ids[i]] = st;
      m_end2st.push_back(make_pair(st->m_points.front(), st));
      m_end2st.push_back(make_pair(st->m_points.back(), st));
    }
  }

  m_loader.Free();
  return count;
}

int HouseDetector::MergeStreets()
{
  LOG(LDEBUG, ("MergeStreets() called", m_id2st.size()));

//#ifdef DEBUG
//  KMLFileGuard file("dbg_merged_streets.kml");
//#endif

  for (StreetMapT::iterator it = m_id2st.begin(); it != m_id2st.end(); ++it)
  {
    Street * st = it->second;

    if (st->m_number == -1)
    {
      MergeStreets(st);
      m_streetNum++;
    }
  }

  // Put longer streets first (for better house scoring).
  sort(m_streets.begin(), m_streets.end(), MergedStreet::GreaterLength());

//#ifdef DEBUG
//  char const * arrColor[] = { "FFFF0000", "FF00FFFF", "FFFFFF00", "FF0000FF", "FF00FF00", "FFFF00FF" };

//  // Write to kml from short to long to get the longest one at the top.
//  for (int i = int(m_streets.size()) - 1; i >= 0; --i)
//  {
//    Streets2KML(file.GetStream(), m_streets[i], arrColor[i % ARRAY_SIZE(arrColor)]);
//  }
//#endif

  LOG(LDEBUG, ("MergeStreets() result", m_streetNum));
  return m_streetNum;
}

namespace
{

class ProjectionCalcToStreet
{
  vector<m2::PointD> const & m_points;
  double m_distanceMeters;

  typedef m2::ProjectionToSection<m2::PointD> ProjectionT;
  vector<ProjectionT> m_calcs;

public:
  ProjectionCalcToStreet(Street const * st, double distanceMeters)
    : m_points(st->m_points), m_distanceMeters(distanceMeters)
  {
    ASSERT_GREATER(m_points.size(), 1, ());
  }

  void Initialize()
  {
    if (m_calcs.empty())
    {
      size_t const count = m_points.size() - 1;
      m_calcs.resize(count);
      for (size_t i = 0; i < count; ++i)
        m_calcs[i].SetBounds(m_points[i], m_points[i+1]);
    }
  }

  double GetLength(size_t ind) const
  {
    double length = 0.0;
    for (size_t i = 0; i < ind; ++i)
      length += m_calcs[i].GetLength();
    return length;
  }

  double GetLength()
  {
    Initialize();
    return GetLength(m_calcs.size());
  }

  void CalculateProjectionParameters(m2::PointD const & pt,
                                     m2::PointD & resPt, double & dist, double & resDist, size_t & ind)
  {
    for (size_t i = 0; i < m_calcs.size(); ++i)
    {
      m2::PointD const proj = m_calcs[i](pt);
      dist = GetDistanceMeters(pt, proj);
      if (dist < resDist)
      {
        resPt = proj;
        resDist = dist;
        ind = i;
      }
    }
  }

  bool GetProjection(m2::PointD const & pt, HouseProjection & proj)
  {
    Initialize();

    m2::PointD resPt;
    double dist = numeric_limits<double>::max();
    double resDist = numeric_limits<double>::max();
    size_t ind;

    CalculateProjectionParameters(pt, resPt, dist, resDist, ind);

    if (resDist <= m_distanceMeters)
    {
      proj.m_proj = resPt;
      proj.m_distance = resDist;
      proj.m_streetDistance = GetLength(ind) + m_points[ind].Length(proj.m_proj);
      proj.m_projectionSign = m2::GetOrientation(m_points[ind], m_points[ind+1], pt) >= 0;
      return true;
    }
    else
      return false;
  }
};

}

string const & MergedStreet::GetDbgName() const
{
  ASSERT(!m_cont.empty(), ());
  return m_cont.front()->GetDbgName();
}

string const & MergedStreet::GetName() const
{
  ASSERT(!m_cont.empty(), ());
  return m_cont.front()->GetName();
}

bool MergedStreet::IsHousesReaded() const
{
  ASSERT(!m_cont.empty(), ());
  return m_cont.front()->m_housesReaded;
}

void MergedStreet::Next(Index & i) const
{
  while (i.s < m_cont.size() && i.h == m_cont[i.s]->m_houses.size())
  {
    i.h = 0;
    ++i.s;
  }
}

void MergedStreet::Erase(Index & i)
{
  ASSERT(!IsEnd(i), ());
  m_cont[i.s]->m_houses.erase(m_cont[i.s]->m_houses.begin() + i.h);
  if (m_cont[i.s]->m_houses.empty())
    m_cont.erase(m_cont.begin() + i.s);
  Next(i);
}

void MergedStreet::FinishReadingHouses()
{
  // Correct m_streetDistance for each projection according to merged streets.
  double length = 0.0;
  for (size_t i = 0; i < m_cont.size(); ++i)
  {
    if (i != 0)
      for (size_t j = 0; j < m_cont[i]->m_houses.size(); ++j)
        m_cont[i]->m_houses[j].m_streetDistance += length;

    length += m_cont[i]->m_length;
    m_cont[i]->m_housesReaded = true;
  }

  // Unique projections for merged street.
  for (Index i = Begin(); !IsEnd(i);)
  {
    HouseProjection const & p1 = Get(i);
    bool incI = true;

    Index j = i; Inc(j);
    while (!IsEnd(j))
    {
      HouseProjection const & p2 = Get(j);
      if (p1.m_house == p2.m_house)
      {
        if (p1.m_distance < p2.m_distance)
        {
          Erase(j);
        }
        else
        {
          Erase(i);
          incI = false;
          break;
        }
      }
      else
        Inc(j);
    }

    if (incI)
      Inc(i);
  }
}

HouseProjection const * MergedStreet::GetHousePivot(bool isOdd, bool & sign) const
{
  typedef my::limited_priority_queue<
      HouseProjection const *, HouseProjection::LessDistance> QueueT;
  QueueT q(HN_COUNT_FOR_ODD_TEST);

  // Get some most closest houses.
  for (MergedStreet::Index i = Begin(); !IsEnd(i); Inc(i))
    q.push(&Get(i));

  // Calculate all probabilities.
  // even-left, odd-left, even-right, odd-right
  double counter[4] = { 0, 0, 0, 0 };
  for (QueueT::const_iterator i = q.begin(); i != q.end(); ++i)
  {
    size_t ind = (*i)->m_house->GetIntNumber() % 2;
    if ((*i)->m_projectionSign)
      ind += 2;

    // Needed min summary distance, but process max function.
    counter[ind] += (1.0 / (*i)->m_distance);
  }

  // Get best odd-sign pair.
  if (counter[0] + counter[3] > counter[1] + counter[2])
    sign = isOdd;
  else
    sign = !isOdd;

  // Get result pivot according to odd-sign pair.
  while (!q.empty())
  {
    HouseProjection const * p = q.top();
    if ((p->m_projectionSign == sign) && (p->IsOdd() == isOdd))
      return p;
    q.pop();
  }

  return 0;
}

uint32_t HouseDetector::GetBuildingType() const
{
  return m_buildingChecker.GetMainType();
}

template <class ProjectionCalcT>
void HouseDetector::ReadHouse(FeatureType const & f, Street * st, ProjectionCalcT & calc)
{
  string const houseNumber = f.GetHouseNumber();

  /// @todo After new data generation we can skip IsHouseNumber check here.
  if (m_buildingChecker(f) && feature::IsHouseNumber(houseNumber))
  {
    HouseMapT::iterator const it = m_id2house.find(f.GetID());
    bool const isNew = it == m_id2house.end();

    m2::PointD const pt = isNew ? f.GetLimitRect(FeatureType::BEST_GEOMETRY).Center() : it->second->GetPosition();

    HouseProjection pr;
    if (calc.GetProjection(pt, pr))
    {
      House * p;
      if (isNew)
      {
        p = new House(houseNumber, pt);
        m_id2house[f.GetID()] = p;
      }
      else
      {
        p = it->second;
        ASSERT(p != 0, ());
      }

      pr.m_house = p;
      st->m_houses.push_back(pr);
    }
  }
}

void HouseDetector::ReadHouses(Street * st, double offsetMeters)
{
  if (st->m_housesReaded)
    return;

  //offsetMeters = max(HN_MIN_READ_OFFSET_M, min(GetApprLengthMeters(st->m_number) / 2, offsetMeters));

  ProjectionCalcToStreet calcker(st, offsetMeters);
  m_loader.ForEachInRect(st->GetLimitRect(offsetMeters),
                         bind(&HouseDetector::ReadHouse<ProjectionCalcToStreet>, this, _1, st, ref(calcker)));

  st->m_length = calcker.GetLength();
  st->SortHousesProjection();
}

void HouseDetector::ReadAllHouses(double offsetMeters)
{
  m_houseOffsetM = offsetMeters;

  for (StreetMapT::iterator it = m_id2st.begin(); it != m_id2st.end(); ++it)
    ReadHouses(it->second, offsetMeters);

  for (size_t i = 0; i < m_streets.size(); ++i)
  {
    if (!m_streets[i].IsHousesReaded())
      m_streets[i].FinishReadingHouses();
  }
}

void HouseDetector::ClearCaches()
{
  for (StreetMapT::iterator it = m_id2st.begin(); it != m_id2st.end(); ++it)
    delete it->second;
  m_id2st.clear();

  for (HouseMapT::iterator it = m_id2house.begin(); it != m_id2house.end(); ++it)
    delete it->second;

  m_streetNum = 0;

  m_id2house.clear();
  m_end2st.clear();
  m_streets.clear();
}

namespace
{

class HasSecond
{
  set<Street *> const & m_streets;
public:
  HasSecond(set<Street *> const & streets) : m_streets(streets) {}
  template <class T> bool operator() (T const & t) const
  {
    return m_streets.count(t.second) > 0;
  }
};

class HasStreet
{
  set<Street *> const & m_streets;
public:
  HasStreet(set<Street *> const & streets) : m_streets(streets) {}
  bool operator() (MergedStreet const & st) const
  {
    for (size_t i = 0; i < st.m_cont.size(); ++i)
      if (m_streets.count(st.m_cont[i]) > 0)
        return true;
    return false;
  }
};

}

void HouseDetector::ClearUnusedStreets(vector<FeatureID> const & ids)
{
  set<Street *> streets;
  for (StreetMapT::iterator it = m_id2st.begin(); it != m_id2st.end();)
  {
    if (!binary_search(ids.begin(), ids.end(), it->first))
    {
      streets.insert(it->second);
      m_id2st.erase(it++);
    }
    else
      ++it;
  }

  m_end2st.erase(remove_if(m_end2st.begin(), m_end2st.end(), HasSecond(streets)), m_end2st.end());
  m_streets.erase(remove_if(m_streets.begin(), m_streets.end(), HasStreet(streets)), m_streets.end());

  for_each(streets.begin(), streets.end(), DeleteFunctor());
}

string DebugPrint(HouseProjection const & p)
{
  return p.m_house->GetNumber();
}

template <class T> void LogSequence(vector<T const *> const & v)
{
#ifdef DEBUG
  for (size_t i = 0; i < v.size(); ++i)
    LOG(LDEBUG, (*v[i]));
#endif
}

namespace
{

struct ScoredHouse
{
  House const * house;
  double score;
  ScoredHouse(House const * h, double s) : house(h), score(s) {}
  ScoredHouse() : house(0), score(numeric_limits<double>::max()) {}
};

class ResultAccumulator
{
  ParsedNumber m_number;
  bool m_isOdd, m_sign, m_useOdd;

  ScoredHouse m_results[4];

  bool IsBetter(int ind, double dist) const
  {
    return m_results[ind].house == 0 || m_results[ind].score > dist;
  }

public:
  ResultAccumulator(string const & houseNumber)
    : m_number(houseNumber)
  {
  }

  string const & GetFullNumber() const { return m_number.GetNumber(); }
  bool UseOdd() const { return m_useOdd; }

  bool SetStreet(MergedStreet const & st)
  {
    Reset();

    m_useOdd = true;
    m_isOdd = m_number.IsOdd();
    return st.GetHousePivot(m_isOdd, m_sign) != 0;
  }

  void SetSide(bool sign)
  {
    Reset();

    m_useOdd = false;
    m_sign = sign;
  }

  void Reset()
  {
    for (size_t i = 0; i < ARRAY_SIZE(m_results); ++i)
      m_results[i] = ScoredHouse();
  }

  void ResetNearby() { m_results[3] = ScoredHouse(); }

  bool IsOurSide(HouseProjection const & p) const
  {
    if (m_sign != p.m_projectionSign)
      return false;
    return (!m_useOdd || m_isOdd == p.IsOdd());
  }

  void ProcessCandidate(HouseProjection const & p)
  {
    if (IsOurSide(p))
      MatchCandidate(p, false);
  }

  void MatchCandidate(HouseProjection const & p, bool checkNearby)
  {
    int ind = p.m_house->GetMatch(m_number);
    if (ind == -1)
    {
      if (checkNearby && p.m_house->GetNearbyMatch(m_number))
        ind = 3;
      else
        return;
    }

    if (IsBetter(ind, p.m_distance))
      m_results[ind] = ScoredHouse(p.m_house, p.m_distance);
  }

  template <class TCont>
  void FlushResults(TCont & cont) const
  {
    size_t const baseScore = 1 << (ARRAY_SIZE(m_results) - 2);

    for (size_t i = 0; i < ARRAY_SIZE(m_results) - 1; ++i)
      if (m_results[i].house)
      {
        // Scores are: 4, 2, 1 according to the matching.
        size_t const score = baseScore >> i;

        size_t j = 0;
        for (; j < cont.size(); ++j)
          if (cont[j].first == m_results[i].house)
          {
            cont[j].second += score;
            break;
          }

        if (j == cont.size())
          cont.push_back(make_pair(m_results[i].house, score));
      }
  }

  House const * GetBestMatchHouse() const
  {
    return m_results[0].house;
  }

  bool HasBestMatch() const { return (m_results[0].house != 0); }
  House const * GetNearbyCandidate() const { return (HasBestMatch() ? 0 : m_results[3].house); }
};
/*
void ProcessNearbyHouses(vector<HouseProjection const *> const & v, ResultAccumulator & acc)
{
  House const * p = acc.GetNearbyCandidate();
  if (p)
  {
    // Get additional search interval.
    int const pivot = distance(v.begin(), find_if(v.begin(), v.end(), HouseProjection::EqualHouse(p)));
    ASSERT(pivot >= 0 && pivot < v.size(), ());
    int const start = max(pivot - HN_NEARBY_INDEX_RANGE, 0);
    int const end = min(pivot + HN_NEARBY_INDEX_RANGE + 1, int(v.size()));

    for (int i = start; i < end; ++i)
      acc.MatchCandidate(*v[i], false);
  }
}
*/
//void GetClosestHouse(MergedStreet const & st, ResultAccumulator & acc)
//{
//  for (MergedStreet::Index i = st.Begin(); !st.IsEnd(i); st.Inc(i))
//    acc.ProcessCandidate(st.Get(i));
//}

// If it's odd or even part of the street pass 2, else pass 1.
void AddToQueue(int houseNumber, int step, queue<int> & q)
{
  for (int i = 1; i <= 2; ++i)
  {
    int const delta = step * i;
    q.push(houseNumber + delta);
    if (houseNumber - delta > 0)
      q.push(houseNumber - delta);
  }
}

struct HouseChain
{
  vector<HouseProjection const *> houses;
  set<string> chainHouses;
  double score;
  int minHouseNumber;
  int maxHouseNumber;

  HouseChain()
  {
    minHouseNumber = -1;
    maxHouseNumber = numeric_limits<int>::max();
  }

  HouseChain(HouseProjection const * h)
  {
    minHouseNumber = maxHouseNumber = h->m_house->GetIntNumber();
    Add(h);
  }

  void Add(HouseProjection const * h)
  {
    if (chainHouses.insert(h->m_house->GetNumber()).second)
    {
      int num = h->m_house->GetIntNumber();
      if (num < minHouseNumber)
        minHouseNumber = num;
      if (num > maxHouseNumber)
        maxHouseNumber = num;
      houses.push_back(h);
    }
  }

  bool Find(string const & str)
  {
    return (chainHouses.find(str) != chainHouses.end());
  }

  void CountScore()
  {
    sort(houses.begin(), houses.end(), HouseProjection::LessDistance());
    size_t const size = houses.size();
    score = 0;
    size_t const scoreNumber = 3;
    for (size_t i = 0; i < scoreNumber; ++i)
      score += i < size ? houses[i]->m_distance : houses.back()->m_distance;
    score /= scoreNumber;
  }

  bool IsIntersecting(HouseChain const & chain) const
  {
    if (minHouseNumber >= chain.maxHouseNumber)
      return false;
    if (chain.minHouseNumber >= maxHouseNumber)
      return false;
    return true;
  }

  bool operator<(HouseChain const & p) const
  {
    return score < p.score;
  }
};

void GetBestHouseFromChains(vector<HouseChain> & houseChains, ResultAccumulator & acc)
{
  for (size_t i = 0; i < houseChains.size(); ++i)
    houseChains[i].CountScore();
  sort(houseChains.begin(), houseChains.end());

  ParsedNumber number(acc.GetFullNumber());

  for (size_t i = 0; i < houseChains.size(); ++i)
  {
    if (i == 0 || !houseChains[0].IsIntersecting(houseChains[i]))
    {
      for (size_t j = 0; j < houseChains[i].houses.size(); ++j)
      {
        if (houseChains[i].houses[j]->m_house->GetMatch(number) != -1)
        {
          for (size_t k = 0; k < houseChains[i].houses.size(); ++k)
            acc.MatchCandidate(*houseChains[i].houses[k], true);
          break;
        }
      }
    }
  }
}


struct Competitiors
{
  int m_candidateIndex;
  int m_chainIndex;
  double m_score;
  Competitiors(int candidateIndex, int chainIndex, double score)
    : m_candidateIndex(candidateIndex), m_chainIndex(chainIndex), m_score(score)
  {}
  bool operator<(Competitiors const & c) const
  {
    return m_score < c.m_score;
  }
};


void ProccessHouses(vector<HouseProjection const *> const & st, ResultAccumulator & acc)
{
  vector<HouseChain> houseChains;
  size_t const count = st.size();
  size_t numberOfStreetHouses = count;
  vector<bool> used(count, false);
  string const & houseNumber = acc.GetFullNumber();
  int const step = acc.UseOdd() ? 2 : 1;

  for (size_t i = 0; i < count; ++i)
  {
    HouseProjection const * hp = st[i];
    if (st[i]->m_house->GetNumber() == houseNumber)
    {
      houseChains.push_back(HouseChain(hp));
      used[i] = true;
      --numberOfStreetHouses;
    }
  }
  if (houseChains.empty())
    return;

  queue<int> houseNumbersToCheck;
  AddToQueue(houseChains[0].houses[0]->m_house->GetIntNumber(), step, houseNumbersToCheck);
  while (numberOfStreetHouses > 0)
  {
    if (!houseNumbersToCheck.empty())
    {
      int candidateHouseNumber = houseNumbersToCheck.front();
      houseNumbersToCheck.pop();
      vector <int> candidates;
      for (size_t i = 0; i < used.size(); ++i)
        if (!used[i] && st[i]->m_house->GetIntNumber() == candidateHouseNumber)
          candidates.push_back(i);

      bool shouldAddHouseToQueue = false;
      vector <Competitiors> comp;

      for (size_t i = 0; i < candidates.size(); ++i)
      {
        string num = st[candidates[i]]->m_house->GetNumber();
        for (size_t j = 0; j < houseChains.size(); ++j)
        {
          if (!houseChains[j].Find(num))
          {
            double dist = numeric_limits<double>::max();
            for (size_t k = 0; k < houseChains[j].houses.size(); ++k)
            {
              if (abs(houseChains[j].houses[k]->m_house->GetIntNumber() - st[candidates[i]]->m_house->GetIntNumber()) <= HN_NEARBY_DISTANCE)
                dist = min(dist, GetDistanceMeters(houseChains[j].houses[k]->m_house->GetPosition(), st[candidates[i]]->m_house->GetPosition()));
            }
            if (dist < HN_MAX_CONNECTION_DIST_M)
              comp.push_back(Competitiors(candidates[i], j, dist));
          }
        }
      }
      sort(comp.begin(), comp.end());

      for (size_t i = 0; i < comp.size(); ++i)
      {
        if (!used[comp[i].m_candidateIndex])
        {
          string num = st[comp[i].m_candidateIndex]->m_house->GetNumber();
          if (!houseChains[comp[i].m_chainIndex].Find(num))
          {
            used[comp[i].m_candidateIndex] = true;
            houseChains[comp[i].m_chainIndex].Add(st[comp[i].m_candidateIndex]);
            --numberOfStreetHouses;
            shouldAddHouseToQueue = true;
          }
        }
      }
      if (shouldAddHouseToQueue)
        AddToQueue(candidateHouseNumber, step, houseNumbersToCheck);
    }
    else
    {
      for (size_t i = 0; i < used.size(); ++i)
      {
        if (!used[i])
        {
          houseChains.push_back(HouseChain(st[i]));
          --numberOfStreetHouses;
          used[i] = true;
          AddToQueue(st[i]->m_house->GetIntNumber(), step, houseNumbersToCheck);
          break;
        }
      }
    }
  }
  GetBestHouseFromChains(houseChains, acc);
}

void GetBestHouseWithNumber(MergedStreet const & st, double offsetMeters, ResultAccumulator & acc)
{
  vector<HouseProjection const *> v;
  for (MergedStreet::Index i = st.Begin(); !st.IsEnd(i); st.Inc(i))
  {
    HouseProjection const & p = st.Get(i);
    if (p.m_distance <= offsetMeters && acc.IsOurSide(p))
      v.push_back(&p);
  }

  ProccessHouses(v, acc);
}

struct CompareHouseNumber
{
  inline bool Less(HouseProjection const * h1, HouseProjection const * h2) const
  {
    return (h1->m_house->GetIntNumber() <= h2->m_house->GetIntNumber());
  }
  inline bool Greater(HouseProjection const * h1, HouseProjection const * h2) const
  {
    return (h1->m_house->GetIntNumber() >= h2->m_house->GetIntNumber());
  }
};
/*
void LongestSubsequence(vector<HouseProjection const *> const & v,
                        vector<HouseProjection const *> & res)
{
  LongestSubsequence(v, back_inserter(res), CompareHouseNumber());
}
*/
//void GetLSHouse(MergedStreet const & st, double offsetMeters, ResultAccumulator & acc)
//{
//  acc.ResetNearby();

//  vector<HouseProjection const *> v;
//  for (MergedStreet::Index i = st.Begin(); !st.IsEnd(i); st.Inc(i))
//  {
//    search::HouseProjection const & p = st.Get(i);
//    if (p.m_distance <= offsetMeters && acc.IsOurSide(p))
//      v.push_back(&p);
//  }

//  vector<HouseProjection const *> res;
//  LongestSubsequence(v, res);

//  //LOG(LDEBUG, ("=== Offset", offsetMeters, "==="));
//  //LogSequence(res);

//  for (size_t i = 0; i < res.size(); ++i)
//    acc.MatchCandidate(*(res[i]), true);

//  ProcessNearbyHouses(v, acc);
//}

struct GreaterSecond
{
  template <class T>
  bool operator() (T const & t1, T const & t2) const { return t1.second > t2.second; }
};

void ProduceVoting(vector<ResultAccumulator> const & acc,
                   vector<HouseResult> & res,
                   MergedStreet const & st)
{
  buffer_vector<pair<House const *, size_t>, 4> voting;

  // Calculate score for every house.
  for (size_t i = 0; i < acc.size(); ++i)
    acc[i].FlushResults(voting);

  if (voting.empty())
    return;

  // Sort according to the score (bigger is better).
  sort(voting.begin(), voting.end(), GreaterSecond());

  // Emit results with equal best score.
  size_t const score = voting[0].second;
  for (size_t i = 0; i < voting.size(); ++i)
  {
    if (score == voting[i].second)
      res.push_back(HouseResult(voting[i].first, &st));
    else
      break;
  }
}

}

void HouseDetector::GetHouseForName(string const & houseNumber, vector<HouseResult> & res)
{
  size_t const count = m_streets.size();
  res.reserve(count);

  LOG(LDEBUG, ("Streets count", count));

  vector<ResultAccumulator> acc(3, ResultAccumulator(houseNumber));

  int offsets[] = { 25, 50, 100, 200, 500 };

  for (size_t i = 0; i < count; ++i)
  {
    LOG(LDEBUG, (m_streets[i].GetDbgName()));

    acc[0].SetStreet(m_streets[i]);
    acc[1].SetSide(true);
    acc[2].SetSide(false);

    // 0.88668
    for (size_t j = 0; j < ARRAY_SIZE(offsets) && offsets[j] <= m_houseOffsetM; ++j)
      for (size_t k = 0; k < acc.size(); ++k)
      {
        GetBestHouseWithNumber(m_streets[i], offsets[j], acc[k]);
        if (acc[k].HasBestMatch())
          goto end;
      }

end:
    ProduceVoting(acc, res, m_streets[i]);

    for (size_t j = 0; j < acc.size(); ++j)
      acc[j].Reset();
  }

  sort(res.begin(), res.end());
  res.erase(unique(res.begin(), res.end()), res.end());
}

}