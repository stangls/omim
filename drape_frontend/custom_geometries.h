#ifndef CUSTOMGEOM_H
#define CUSTOMGEOM_H

#include "drape/color.hpp"
#include "geometry/rect2d.hpp"
#include "base/logging.hpp"

namespace df
{

class CustomGeom
{
public:
    CustomGeom( m2::RectD outerRect, dp::Color color );

    const dp::Color GetColor() const { return m_color; }
    const m2::RectD GetBoundingBox() const { return m_outerRect; }

private:
    m2::RectD m_outerRect;
    dp::Color m_color;
};

class CustomGeometries
{
private:
    static CustomGeometries* s_inst;
    CustomGeometries();
    vector<CustomGeom> m_geoms;
public:
    static CustomGeometries* GetInstance() {
        if (!s_inst){
            s_inst=new CustomGeometries();
        }
        return s_inst;
    }
    vector<CustomGeom*> GetGeometries( m2::RectD rectangle );

};

} // end namespace df

#endif // CUSTOMGEOM_H
