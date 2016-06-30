#include "custom_geometries.h"
#include "geometry/mercator.hpp"

namespace df
{


CustomGeom::CustomGeom(m2::RectD outerRect, dp::Color color) :
    m_outerRect(outerRect),
    m_color(color)
{

}

vector<CustomGeom*> CustomGeometries::GetGeometries(m2::RectD rectangle)
{
    vector<CustomGeom*> ret;
    for ( auto it=m_geoms.cbegin(); it!=m_geoms.cend(); it++ ){
        CustomGeom g = *it;
        if (rectangle.IsPointInside(g.GetBoundingBox().Center())){
            ret.push_back( &g );
        }
    }
    return ret;
}

CustomGeometries::CustomGeometries()
{
    LOG(my::LINFO,("loading custom geometries"));
    m_geoms.clear();
    m_geoms.emplace_back(
        m2::RectD(
          MercatorBounds::LonToX(12.120659), MercatorBounds::LatToY(47.797162),
          MercatorBounds::LonToX(12.120981), MercatorBounds::LatToY(47.797328)
        ),
        dp::Color::Red()
    );

}

} // end namespace df
