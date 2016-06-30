#include "custom_geometries.h"
#include "geometry/mercator.hpp"

namespace df
{

// Custom Geometry

CustomGeom::CustomGeom(m2::RectD outerRect, dp::Color color) :
    m_outerRect(outerRect),
    m_color(color)
{}

vector<shared_ptr<CustomGeom>> const CustomGeometries::GetGeometries(m2::RectD rectangle)
{
    vector<shared_ptr<CustomGeom>> ret;
    for ( auto it=m_geoms.cbegin(); it!=m_geoms.cend(); it++ ){
        shared_ptr<CustomGeom> g = *it;
        if (rectangle.IsPointInside(g->GetBoundingBox().Center()) || rectangle.IsIntersect(g->GetBoundingBox())){
            ret.push_back( g );
        }
    }
    return ret;
}


// Custom Geometries repository

CustomGeometries::CustomGeometries()
{
    ReloadGeometries();
}
void CustomGeometries::ReloadGeometries(){
    LOG(my::LINFO,("loading custom geometries"));
    m_geoms.clear();
    m_geoms.push_back(shared_ptr<CustomGeom>(
        new CustomGeom(
          m2::RectD(
            MercatorBounds::LonToX(12.120659), MercatorBounds::LatToY(47.797162),
            MercatorBounds::LonToX(12.120981), MercatorBounds::LatToY(47.797328)
          ),
          dp::Color::Red()
        )
    ));

}

CustomGeometries* CustomGeometries::s_inst=0;

} // end namespace df
