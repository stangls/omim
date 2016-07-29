#include "custom_geometries.h"
#include "geometry/mercator.hpp"

// includes for parser
#include "coding/parse_xml.hpp"
#include "coding/file_reader.hpp"
#include "coding/reader.hpp"
#include "base/string_utils.hpp"

namespace df
{

// Convex Geometry

ConvexGeom::ConvexGeom(vector<m2::PointF> &outerPoints, dp::Color& color) :
    CustomGeom(),
    m_outerPoints(outerPoints)
{
    m_outerRect.MakeEmpty();
    for (auto it=m_outerPoints.cbegin(); it!=m_outerPoints.cend(); it++){
        m_outerRect.Add(*it);
    };
    m_color = color;
}

void callInOrder(TPolyFun callback, m2::PointF p1, m2::PointF p2, m2::PointF p3){
    double const kEps = 1e-7;
    // logic copied from apply_feature_functors.cpp : ApplyAreaFeature::operator()
    m2::PointD const v1 = p2 - p1;
    m2::PointD const v2 = p3 - p1;
    if (v1.IsAlmostZero() || v2.IsAlmostZero())
      return;
    double const crossProduct = m2::CrossProduct(v1.Normalize(), v2.Normalize());
    if (fabs(crossProduct) < kEps)
      return;
    if (m2::CrossProduct(v1, v2) < 0)
        callback( p1, p2, p3 );
    else
        callback( p1, p3, p2 );
}

void ConvexGeom::CreatePolys(TPolyFun callback)
{
    LOG(my::LINFO,("creating other polygons"));
    if (m_outerPoints.size()<3)
        return;
    auto it = m_outerPoints.cbegin();
    auto p1 = *(it++);
    auto p2 = *(it++);
    while (it!=m_outerPoints.cend()){
        // iterate
        auto p3=*(it++);
        callInOrder(callback,p1,p2,p3);
        // iterate
        p2=p3;
    }
}

// TriangleGeom

TriangleGeom::TriangleGeom(vector<m2::PointF> &points, vector<size_t> triangles, dp::Color &color)
    : CustomGeom(),
      m_points(points),
      m_triangles(triangles)
{
    m_outerRect.MakeEmpty();
    for (auto it=m_points.cbegin(); it!=m_points.cend(); it++){
        m_outerRect.Add(*it);
    };
    m_color = color;
}

void TriangleGeom::CreatePolys(TPolyFun callback)
{
    ASSERT( m_triangles.size()%3==0, () );
    for (auto it = m_triangles.begin(); it!=m_triangles.end(); it++){
        callInOrder( callback, m_points[*(it++)], m_points[*(it++)], m_points[*it] );
    }
}

// anonymous namespace for parses
namespace{

class GeomsParser
{
    using Geoms = CustomGeometries::Geoms;
    Geoms &m_geoms;
    vector<string> m_tags;

    float m_px, m_py;
    int m_p1, m_p2, m_p3;
    vector<m2::PointF> m_points;
    vector<size_t> m_triangles;
    dp::Color m_color;

    void Reset() { }

public:
    GeomsParser(Geoms & geoms)
        : m_geoms(geoms)
    { Reset(); }

    ~GeomsParser() {}

    bool Push(string const & tag)
    {
        m_tags.push_back(tag);
        size_t const count = m_tags.size();
        string const & currTag = m_tags[count - 1];
        if (count > 1)
        {
            string const & prevTag = m_tags[count - 2];
        }
        if (tag=="trianglelist"){
            m_points.clear();
            m_triangles.clear();
            m_color=dp::Color::Black();
        }
        return true;
    }

    void AddAttr(string const & attr, string const & value)
    {
        //LOG( my::LINFO, ("attr",attr,"=",value) );
        string attrInLowerCase = attr;
        strings::AsciiToLower(attrInLowerCase);

        if (IsValidAttribute("trianglelist", attrInLowerCase, "color", value)){
            m_color = dp::Extract(atoi(value.c_str()));
        }
        if (IsValidAttribute("point", attrInLowerCase, "x", value)){
            m_px = atof(value.c_str());
        }
        if (IsValidAttribute("point", attrInLowerCase, "y", value)){
            m_py = atof(value.c_str());
        }
        if (IsValidAttribute("triangle", attrInLowerCase, "p1", value))
            m_p1 = atoi(value.c_str());
        if (IsValidAttribute("triangle", attrInLowerCase, "p2", value))
            m_p2 = atoi(value.c_str());
        if (IsValidAttribute("triangle", attrInLowerCase, "p3", value))
            m_p3 = atoi(value.c_str());
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
        if (tag=="trianglelist") {
            CustomGeom *cg = (CustomGeom*)new TriangleGeom( m_points, m_triangles, m_color );
            LOG(my::LINFO,("read geometry",*cg));
            m_geoms.push_back( shared_ptr<CustomGeom>( cg ) );
            m_points.clear();
            m_triangles.clear();
        }
        if (tag=="point") {
            m_points.emplace_back( MercatorBounds::LonToX(m_px), MercatorBounds::LatToY(m_py) );
        }
        if (tag=="triangle") {
            m_triangles.emplace_back( m_p1 );
            m_triangles.emplace_back( m_p2 );
            m_triangles.emplace_back( m_p3 );
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
            /*
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
            }*/
        }
    }
};
} // end anonymous namespace (parser)

// Custom Geometries repository

CustomGeometries::Geoms const CustomGeometries::GetGeometries(m2::RectD rectangle)
{
    lock_guard<mutex> lock(m_mutex);
    vector<shared_ptr<CustomGeom>> ret;
    for ( auto it=m_geoms.cbegin(); it!=m_geoms.cend(); it++ ){
        shared_ptr<CustomGeom> g = *it;
        if (rectangle.IsPointInside(g->GetBoundingBox().Center())/* || rectangle.IsIntersect(g->GetBoundingBox())*/){
            ret.push_back( g );
        }
    }
    return ret;
}


CustomGeometries::CustomGeometries()
{
    ReloadGeometries();
}
void CustomGeometries::ReloadGeometries(){
    Geoms new_geoms;
    if (!m_geomsXmlFile.empty()){
        LOG( my::LINFO, ("reading geometries from file ",m_geomsXmlFile) );
        FileReader reader(m_geomsXmlFile);
        ReaderSource<FileReader> src(reader);
        GeomsParser parser(new_geoms);
        ParseXML(src, parser, true);
        LOG( my::LINFO, ("done reading",new_geoms.size(),"geometries from file") );
        lock_guard<mutex> lock(m_mutex);
        m_geoms = new_geoms;
    }
}
CustomGeometries* CustomGeometries::s_inst=0;

} // end namespace df
