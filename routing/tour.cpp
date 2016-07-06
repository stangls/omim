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
    int m_roadIndex=-1;
    bool m_roundabout=false;
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
        if (IsValidAttribute("junction", attrInLowerCase, "type", value)){
            if (value=="roundabout"){
                m_roundabout=true;
            }
        }
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
            //LOG( my::LINFO, ("adding junction") );
            turns::TurnDirection turnDirection=turns::TurnDirection::NoTurn;
            if (m_roundabout){
                //LOG( my::LINFO, ("junction roundabout road index : ",m_roadIndex) );
                m_tour.AddTurn(TI(m_tour.GetAllPoints().size()-1-5,turns::TurnDirection::EnterRoundAbout,m_roadIndex+1));
                turnDirection=turns::TurnDirection::LeaveRoundAbout;
            }else{
                //LOG( my::LINFO, ("junction road angle : ",m_roadAngle) );
                turnDirection = Tour::GetTurnDirectionForAngle(m_roadAngle);
            }
            m_tour.AddTurn(TI(m_tour.GetAllPoints().size()-1,turnDirection,m_roadIndex+1));
            m_roadIndex = -1;
            m_roundabout = false;
        }
        if (tag=="poi"){
            //LOG( my::LINFO, ("adding POI") );
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
                //LOG( my::LINFO, ("tour name : ",value) );
                m_tour.SetName(value);
            }
            if (prevTag=="poi" && currTag=="message"){
                m_poiText=value;
            }
            if (prevTag=="section" && currTag=="name"){
                m_tour.AddStreetname(value);
            }
        }
    }
};
} // end anonymous namespace (parser)

TD Tour::GetTurnDirectionForAngle(int roadAngle){
    if (roadAngle>=175)
        return TD::UTurnLeft;
    else if (roadAngle>=120)
        return TD::TurnSharpRight;
    else if (roadAngle>=45)
        return TD::TurnRight;
    else if (roadAngle>=8)
        return TD::TurnSlightRight;
    else if (roadAngle<=-175)
        return TD::UTurnLeft;
    else if (roadAngle<=-120)
        return TD::TurnSharpLeft;
    else if (roadAngle<=-45)
        return TD::TurnLeft;
    else if (roadAngle<=-8)
        return TD::TurnSlightLeft;
    else
        return TD::GoStraight;
}

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
    LOG( my::LINFO, ("done parsing tour file",filePath,"with tour named",GetName()) );

    LOG( my::LINFO, ("calculating times.") );
    CalculateTimes();
    if (m_turns.size()>0) {
        LOG( my::LINFO, ("removing last turndirection") );
        m_turns.pop_back();
    }
    LOG( my::LINFO, ("placing last turndirection") );
    m_turns.emplace_back( m_points.size()-1, turns::TurnDirection::ReachedYourDestination );

    ASSERT( m_points.size()==m_times.size(), () );
    ASSERT( m_turns.back().m_index == m_points.size()-1, () );

    if (m_streets.size()>0){
        LOG( my::LINFO, ("placing last streetname") );
        AddStreetname(m_streets.back().second);
    }
}

Tour::~Tour()
{
}

string Tour::GetName() const {
    return string(m_name);
}

void Tour::SetName(string name){
    m_name = string(name);
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
                m_nextPoiIndex++;
                if (m_poiVisitedCallback!=0){
                    m_poiVisitedCallback(*poi);
                }
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

void Tour::AddStreetname(string name){
    size_t nextIndex = 0;
    if (m_streets.size()>0){
        nextIndex = m_points.size()-1;
        size_t lastIndex = m_streets.back().first;
        string lastName = m_streets.back().second;
        unsigned int step = MIN_STREETNAME_DIST/MIN_POINT_DIST;
        //LOG(my::LDEBUG,("for loop from ",lastIndex,"+",step,"=",lastIndex+step," to ",nextIndex," by ",step));
        for (size_t i=lastIndex+step; i<nextIndex; i+=step){
            LOG(my::LDEBUG,("adding street",lastName,"at",i));
            m_streets.emplace_back(i,lastName);
        }
    }
    LOG(my::LDEBUG,("adding final street",name,"at",nextIndex));
    m_streets.emplace_back(nextIndex,name);
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

