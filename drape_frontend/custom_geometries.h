#ifndef CUSTOMGEOM_H
#define CUSTOMGEOM_H

#include "drape/color.hpp"
#include "geometry/rect2d.hpp"
#include "base/logging.hpp"

using std::shared_ptr;

namespace df
{


class CustomGeom
{
public:
    CustomGeom( m2::RectD outerRect, dp::Color color );

    dp::Color GetColor() const { return m_color; }
    m2::RectD GetBoundingBox() const { return m_outerRect; }

private:
    m2::RectD m_outerRect;
    dp::Color m_color;
};


inline string DebugPrint(CustomGeom const & c)
{
  ostringstream out;
  out << "BoundingBox = " << DebugPrint(c.GetBoundingBox());
  out << "Color = ( " << DebugPrint(c.GetColor()) << " ) ";
  return out.str();
}

class CustomGeometries
{
private:
    static CustomGeometries* s_inst;
    CustomGeometries();
    CustomGeometries( const CustomGeometries& ); // no copy constructor
    ~CustomGeometries () { }

    vector<shared_ptr<CustomGeom>> m_geoms;
public:
    static CustomGeometries* GetInstance() {
        if (!s_inst){
            s_inst=new CustomGeometries();
        }
        return s_inst;
    }
    vector<shared_ptr<CustomGeom>> const GetGeometries( m2::RectD rectangle );
    void ReloadGeometries();
};

} // end namespace df

#endif // CUSTOMGEOM_H
