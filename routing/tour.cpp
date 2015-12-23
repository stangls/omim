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

    bool Push(string const & name)
    {
        LOG( my::LINFO, ("push",name) );
        m_tags.push_back(name);
        return true;
    }

    void AddAttr(string const & attr, string const & value)
    {
        LOG( my::LINFO, ("attr",attr,"=",value) );
        string attrInLowerCase = attr;
        strings::AsciiToLower(attrInLowerCase);

        if (IsValidAttribute("position", attrInLowerCase, "x", value))
            m_x = atof(value.c_str());
        if (IsValidAttribute("position", attrInLowerCase, "y", value))
            m_y = atof(value.c_str());
        if (IsValidAttribute("junction", attrInLowerCase, "road_index", value))
            m_roadIndex = atoi(value.c_str());
        if (IsValidAttribute("road", attrInLowerCase, "angle", value))
            m_roadAngle = atoi(value.c_str());
    }

    bool IsValidAttribute(string const & type, string const & attrInLowerCase, string const & attrShouldBe, string const & value) const
    {
        return (GetTagFromEnd(0) == type && !value.empty() && attrInLowerCase == attrShouldBe);
    }

    string const & GetTagFromEnd(size_t n) const
    {
        ASSERT_LESS(n, m_tags.size(), ());
        return m_tags[m_tags.size() - n - 1];
    }

    void Pop(string const & tag)
    {
        LOG( my::LINFO, ("pop",tag) );
        ASSERT_EQUAL(m_tags.back(), tag, ());
        if (tag=="position"){
            m_tour.GetAllPoints().emplace_back(MercatorBounds::LonToX(m_x),MercatorBounds::LonToY(m_y));
        }
        if (tag=="section"){
            // TODO: add turn definition
            m_roadIndex = 0;
        }
        m_tags.pop_back();
    }

    void CharData(string value)
    {
        LOG( my::LINFO, ("char data",value) );
        strings::Trim(value);

        size_t const count = m_tags.size();
        if (count > 1 && !value.empty())
        {
            string const & currTag = m_tags[count - 1];
            string const & prevTag = m_tags[count - 2];
            string const ppTag = count > 3 ? m_tags[count - 3] : string();
            // TODO
        }
    }
};
} // end anonymous namespace (parser)


Tour::Tour(const string &filePath)
    : m_currentIndex(0),
      m_points(),
      m_times()
{
    // parse the file

    LOG( my::LINFO, ("reading input file") );
    ReaderPtr<Reader> const & reader = new FileReader(filePath);
    LOG( my::LINFO, ("reading input file (2)",filePath) );
    ReaderSource<ReaderPtr<Reader> > src(reader);
    LOG( my::LINFO, ("instatiating parser") );
    TourParser parser(*this);
    LOG( my::LINFO, ("parsing input file") );
    ParseXML(src, parser, true);
    LOG( my::LINFO, ("done parsing.") );

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

    LOG( my::LINFO, ("calculating times.") );
    CalculateTimes();
    LOG( my::LINFO, ("placing last turndirection") );
    m_turns.emplace_back(m_points.size()-1,turns::TurnDirection::ReachedYourDestination);
}

Tour::~Tour()
{
}

string Tour::GetName(){
    return "dummy tour";
}

vector<PointD> &Tour::GetAllPoints(){
    return m_points;
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

