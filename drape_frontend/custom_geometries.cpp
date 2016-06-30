#include "custom_geometries.h"
#include "drape/color.hpp"

namespace df
{


CustomGeom::CustomGeom(m2::RectD outerRect, dp::Color color) :
    m_outerRect(outerRect),
    m_color(color)
{

}

} // end namespace df
