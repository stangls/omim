#include "custom_geometries.h"
#include "geometry/mercator.hpp"

namespace df
{

// Custom Geometry

CustomGeom::CustomGeom(vector<m2::PointF> &outerPoints, dp::Color& color) :
    m_outerPoints(outerPoints),
    m_color(color)
{
    m_outerRect.MakeEmpty();
    for (auto it=m_outerPoints.cbegin(); it!=m_outerPoints.cend(); it++){
        m_outerRect.Add(*it);
    }
}

void CustomGeom::CreatePolys(TPolyFun callback)
{
    LOG(my::LINFO,("creating other polygons"));
    if (m_outerPoints.size()<3)
        return;
    double const kEps = 1e-7;
    auto it = m_outerPoints.cbegin();
    auto p1 = *(it++);
    auto p2 = *(it++);
    while (it!=m_outerPoints.cend()){
        // iterate
        auto p3=*(it++);

        // logic copied from apply_feature_functors.cpp : ApplyAreaFeature::operator()
        m2::PointD const v1 = p2 - p1;
        m2::PointD const v2 = p3 - p1;
        if (v1.IsAlmostZero() || v2.IsAlmostZero())
          continue;
        double const crossProduct = m2::CrossProduct(v1.Normalize(), v2.Normalize());
        if (fabs(crossProduct) < kEps)
          continue;
        if (m2::CrossProduct(v1, v2) < 0)
            callback( p1, p2, p3 );
        else
            callback( p1, p3, p2 );

        // iterate
        p2=p3;
    }
}

// Custom Geometries repository

vector<shared_ptr<CustomGeom>> const CustomGeometries::GetGeometries(m2::RectD rectangle)
{
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
    LOG(my::LINFO,("loading custom geometries"));
    m_geoms.clear();
    auto color = dp::Color(255,122,0,50);
    vector<m2::PointF> points;
    points.emplace_back(MercatorBounds::LonToX(12.120659), MercatorBounds::LatToY(47.797162));
    points.emplace_back(MercatorBounds::LonToX(12.120820), MercatorBounds::LatToY(47.797162));
    points.emplace_back(MercatorBounds::LonToX(12.120981), MercatorBounds::LatToY(47.797328));
    points.emplace_back(MercatorBounds::LonToX(12.120659), MercatorBounds::LatToY(47.797328));
    m_geoms.push_back(shared_ptr<CustomGeom>(
        new CustomGeom( points, color )
    ));

}

CustomGeometries* CustomGeometries::s_inst=0;

} // end namespace df
