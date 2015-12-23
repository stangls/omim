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

class TourParser
{
    Tour &m_tour; // tour to be constructed / modified
    vector<string> m_tags;

    PointD m_curPosition;
    int m_roadAngle=0;
    int m_roadIndex=0;
    double m_x=0;
    double m_y=0;

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
            LOG( my::LINFO, ("adding point from XML:",m_x,m_y) );
            m_tour.GetAllPoints().emplace_back(MercatorBounds::LonToX(m_x),MercatorBounds::LatToY(m_y));
        }
        if (tag=="section") {
            LOG( my::LINFO, ("adding junction") );
            turns::TurnDirection turnDirection=turns::TurnDirection::NoTurn;
            if (m_roadIndex!=0){
                turnDirection=turns::TurnDirection::LeaveRoundAbout;
            }else{
                if (m_roadAngle>=170)
                    turnDirection=TD::UTurnLeft;
                else if (m_roadAngle>=90)
                    turnDirection=TD::TurnSharpRight;
                else if (m_roadAngle>=15)
                    turnDirection=TD::TurnRight;
                else if (m_roadAngle>=4)
                    turnDirection=TD::TurnSlightRight;
                else if (m_roadAngle<=-170)
                    turnDirection=TD::UTurnLeft;
                else if (m_roadAngle<=-90)
                    turnDirection=TD::TurnSharpLeft;
                else if (m_roadAngle<=-15)
                    turnDirection=TD::TurnLeft;
                else if (m_roadAngle<=-4)
                    turnDirection=TD::TurnSlightLeft;
                else
                    turnDirection=TD::GoStraight;
            }
            m_tour.GetAllTurns().emplace_back(m_tour.GetAllPoints().size(),turnDirection,m_roadIndex);
            m_roadIndex = 0;
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
        }
    }
};
} // end anonymous namespace (parser)


Tour::Tour(const string &filePath)
    : m_currentIndex(0),
      m_points(),
      m_times(),
      m_turns(),
      m_name("unnamed dummy tour")
{

    // parse the file

    LOG( my::LINFO, ("reading tour file ",filePath) );
    ReaderPtr<Reader> const & reader = new FileReader(filePath);
    ReaderSource<ReaderPtr<Reader> > src(reader);
    TourParser parser(*this);
    ParseXML(src, parser, true);
    LOG( my::LINFO, ("done parsing tour file",filePath) );
/*
    m_points.push_back( PointD( MercatorBounds::LonToX(12.123192), MercatorBounds::LatToY(47.796597) ) ); // Angerer Kurve (NO) / Einfahrt Mondi Inncoat
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121801), MercatorBounds::LatToY(47.797094) ) ); // Angerer (N)
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120500), MercatorBounds::LatToY(47.797566) ) ); // Kreuzung Angerer / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120908), MercatorBounds::LatToY(47.798967) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121029), MercatorBounds::LatToY(47.799725) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120902), MercatorBounds::LatToY(47.800527) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120513), MercatorBounds::LatToY(47.801354) ) ); // Kreuzung Eichenweg / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121119), MercatorBounds::LatToY(47.801512) ) ); // Eichenweg Richtung O
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121227), MercatorBounds::LatToY(47.801654) ) ); // Eichenweg Kurve
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121151), MercatorBounds::LatToY(47.802211) ) ); // Eichenweg Richtung N
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120991), MercatorBounds::LatToY(47.802656) ) ); // Kreuzung Eichenweg / Eschenweg
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120404), MercatorBounds::LatToY(47.802446) ) ); // Eschenweg Richtung W
    m_points.push_back( PointD( MercatorBounds::LonToX(12.119868), MercatorBounds::LatToY(47.802172) ) ); // Kreuzung Eschenweg / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120449), MercatorBounds::LatToY(47.801371) ) ); // Kreuzung Eichenweg / B15
*/
    LOG( my::LINFO, ("calculating times.") );
    CalculateTimes();
    LOG( my::LINFO, ("placing last turndirection") );
    m_turns.emplace_back(m_points.size()-1,turns::TurnDirection::ReachedYourDestination);
}

Tour::~Tour()
{
}

bool Tour::UpdateCurrentPosition( size_t index ){
    if (index>=m_points.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
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

