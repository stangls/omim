#include "tour.hpp"
#include "geometry/point2d.hpp"
#include "geometry/mercator.hpp"

// includes for parser
#include "coding/parse_xml.hpp"
#include "coding/file_reader.hpp"
#include "coding/reader.hpp"
#include "base/string_utils.hpp"
//#include <stdlib.h>


namespace routing
{

namespace{ // anonymous namespace for parses

using TD = turns::TurnDirection;
using TI = turns::TurnItem;

class TourParser
{
    Tour &m_tour; // tour to be constructed / modified
    vector<string> m_tags;

    PointD m_curPosition;
    int m_roadAngle=0;
    int m_roadIndex=0;
    double m_x=0;
    double m_y=0;
    bool m_tracePosition=false;
    string m_poiText;

    void Reset()
    { }

public:
    TourParser(Tour & tour)
        : m_tour(tour)
    {
        Reset();
    }

    ~TourParser()
    {}

    bool Push(string const & tag)
    {
        m_tags.push_back(tag);
        size_t const count = m_tags.size();
        if (count > 1)
        {
            string const & currTag = m_tags[count - 1];
            string const & prevTag = m_tags[count - 2];
            if (currTag=="position" && prevTag=="trace"){
                m_tracePosition=true;
            }
        }
        return true;
    }

    void AddAttr(string const & attr, string const & value)
    {
        //LOG( my::LINFO, ("attr",attr,"=",value) );
        string attrInLowerCase = attr;
        strings::AsciiToLower(attrInLowerCase);

        if (IsValidAttribute("position", attrInLowerCase, "x", value)){
            m_x = atof(value.c_str());
        }
        if (IsValidAttribute("position", attrInLowerCase, "y", value)){
            m_y = atof(value.c_str());
        }
        if (IsValidAttribute("junction", attrInLowerCase, "road_index", value))
            m_roadIndex = atoi(value.c_str());
        if (IsValidAttribute("road", attrInLowerCase, "angle", value))
            m_roadAngle = atoi(value.c_str());
    }

    bool IsValidAttribute(string const & tag, string const & attrInLowerCase, string const & attrShouldBe, string const & value) const
    {
        return (GetTagFromEnd(0) == tag && !value.empty() && attrInLowerCase == attrShouldBe);
    }

    string const & GetTagFromEnd(size_t n) const
    {
        ASSERT_LESS(n, m_tags.size(), ());
        return m_tags[m_tags.size() - n - 1];
    }

    void Pop(string const & tag)
    {
        ASSERT_EQUAL(m_tags.back(), tag, ());
        if (tag=="position") {
            auto points = m_tour.GetAllPoints();
            //LOG( my::LINFO, ("adding point (",points.size(),") from XML:",m_x,m_y) );
            if (m_tracePosition){
                m_tour.AddPoint(m_x,m_y);
                m_tracePosition=false;
            }
        }
        if (tag=="section") {
            LOG( my::LINFO, ("adding junction") );
            turns::TurnDirection turnDirection=turns::TurnDirection::NoTurn;
            if (m_roadIndex!=0){
                turnDirection=turns::TurnDirection::LeaveRoundAbout;
            }else{
                if (m_roadAngle>=175)
                    turnDirection=TD::UTurnLeft;
                else if (m_roadAngle>=120)
                    turnDirection=TD::TurnSharpRight;
                else if (m_roadAngle>=45)
                    turnDirection=TD::TurnRight;
                else if (m_roadAngle>=5)
                    turnDirection=TD::TurnSlightRight;
                else if (m_roadAngle<=-175)
                    turnDirection=TD::UTurnLeft;
                else if (m_roadAngle<=-120)
                    turnDirection=TD::TurnSharpLeft;
                else if (m_roadAngle<=-45)
                    turnDirection=TD::TurnLeft;
                else if (m_roadAngle<=-5)
                    turnDirection=TD::TurnSlightLeft;
                else
                    turnDirection=TD::GoStraight;
            }
            m_tour.AddTurn(TI(m_tour.GetAllPoints().size()-1,turnDirection,m_roadIndex));
            m_roadIndex = 0;
        }
        if (tag=="poi"){
            LOG( my::LINFO, ("adding POI") );
            m_tour.AddPoi(m_poiText,m_x,m_y);
            m_poiText="";
        }
        m_tags.pop_back();
    }

    void CharData(string value)
    {
        strings::Trim(value);

        size_t const count = m_tags.size();
        if (count > 1 && !value.empty())
        {
            string const & currTag = m_tags[count - 1];
            string const & prevTag = m_tags[count - 2];
            string const ppTag = count > 3 ? m_tags[count - 3] : string();
            if (prevTag=="tour" && currTag=="name"){
                m_tour.SetName(value);
            }
            if (prevTag=="poi" && currTag=="message"){
                m_poiText=value;
            }
        }
    }
};
} // end anonymous namespace (parser)


Tour::Tour(const string &filePath, TPoiCallback const & poiVisited)
    : m_name("unnamed dummy tour"),
      m_currentIndex(0),
      m_points(),
      m_times(),
      m_turns(),
      m_pois()
{

    m_poiVisitedCallback = poiVisited;

    // parse the XML file
    LOG( my::LINFO, ("reading tour file ",filePath) );
    FileReader reader(filePath);
    ReaderSource<FileReader> src(reader);
    TourParser parser(*this);
    ParseXML(src, parser, true);
    LOG( my::LINFO, ("done parsing tour file",filePath) );

    LOG( my::LINFO, ("calculating times.") );
    CalculateTimes();
    if (m_turns.size()>0) {
        LOG( my::LINFO, ("removing last turndirection") );
        m_turns.pop_back();
    }
    LOG( my::LINFO, ("placing last turndirection") );
    m_turns.emplace_back(m_points.size()-1,turns::TurnDirection::ReachedYourDestination);

    ASSERT( m_points.size()==m_times.size(), () );
    ASSERT( m_turns.back().m_index == m_points.size()-1, () );
}

Tour::~Tour()
{
}

bool Tour::UpdateCurrentPosition( size_t index )
{
    size_t start = m_currentIndex;
    bool ret = JumpCurrentPosition( index );

    // iterate over skipped points and check if POIs exist.
    auto poi = m_pois.cbegin()+m_nextPoiIndex;
    for (size_t idx=start; idx<m_currentIndex; idx++) {
        // check all next POIs of this section
        while (poi != m_pois.cend() && (*poi).GetEarliestIndex()<idx) {
            PointD point = m_points[idx];
            // TODO: actually check POI position and inform GUI
            double dist = MercatorBounds::DistanceOnEarth((*poi).GetPos(),point);
            //LOG( my::LDEBUG, ("checking POI ",m_nextPoiIndex," with distance ",dist," : ",(*poi).GetMessage()));
            if (dist<=MAX_POI_DIST) {
                LOG( my::LDEBUG, ("visiting POI : ",(*poi).GetMessage()));
                if (m_poiVisitedCallback!=0){
                    m_poiVisitedCallback(*poi);
                }
                m_nextPoiIndex++;
            }
            poi++;
        }
    }

    // return actual value
    return ret;
}

bool Tour::JumpCurrentPosition( size_t index )
{
    if (index>=m_points.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
}

void Tour::AddPoint(double lat, double lon)
{
    // minimal distance between points
    // if the last point is further away, additional points are added in-between (to minimize errors of route-following)
    PointD newPoint = PointD(MercatorBounds::LonToX(lat),MercatorBounds::LatToY(lon));
    if (m_points.size()>0){
        PointD lastPoint = m_points.back();
        int steps = (int)ceil(MercatorBounds::DistanceOnEarth(lastPoint,newPoint)/MIN_POINT_DIST)-1;
        auto vec = ( newPoint-lastPoint )/(steps+1);
        for (int i=0; i<steps; i++){
            lastPoint += vec;
            m_points.push_back(lastPoint);
        }
    }
    m_points.emplace_back(newPoint);
}
void Tour::AddPoi(const string & message, double lat, double lon)
{
    m_pois.emplace_back(message,lat,lon,m_points.size());
}


void Tour::CalculateTimes()
{
    if (m_points.size()==0)
        return;

  double constexpr KMPH2MPS = 1000.0 / (60 * 60);
  double const speedMPS = 50 * KMPH2MPS;

  m_times.reserve(m_points.size());

  double trackTimeSec = 0.0;
  m_times.emplace_back(trackTimeSec);

  m2::PointD prev = m_points[0];
  for (size_t i = 1; i < m_points.size(); ++i)
  {
    m2::PointD const & curr = m_points[i];

    double const lengthM = MercatorBounds::DistanceOnEarth(prev, curr);
    trackTimeSec += lengthM / speedMPS;

    m_times.emplace_back(trackTimeSec);

    prev = curr;
  }
}

} // end namespace routing

