#ifndef CUSTOMGEOM_H
#define CUSTOMGEOM_H

#include "drape/color.hpp"
#include "geometry/rect2d.hpp"
#include "base/logging.hpp"

using std::shared_ptr;

namespace df
{

using TPolyFun = function<void (m2::PointF,m2::PointF,m2::PointF)>;

class CustomGeom
{
public:
    CustomGeom(vector<m2::PointF> &outerPoints, dp::Color &color );

    dp::Color GetColor() const { return m_color; }
    m2::RectF GetBoundingBox() const { return m_outerRect; }
    void CreatePolys(TPolyFun callback );

private:
    vector<m2::PointF> m_outerPoints;
    m2::RectF m_outerRect;
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
    vector<shared_ptr<CustomGeom>> const GetGeometries(m2::RectD rectangle );
    void ReloadGeometries();
};

} // end namespace df

#endif // CUSTOMGEOM_H
