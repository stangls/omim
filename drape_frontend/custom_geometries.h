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

    dp::Color GetColor() const { return m_color; }
    m2::RectF GetBoundingBox() const { return m_outerRect; }
    virtual void CreatePolys(TPolyFun callback ) = 0;

protected:
    m2::RectF m_outerRect;
    dp::Color m_color;
};

class ConvexGeom : CustomGeom
{
public:
    ConvexGeom(vector<m2::PointF> &outerPoints, dp::Color &color );
    void CreatePolys(TPolyFun callback );
private:
    vector<m2::PointF> m_outerPoints;
};

class TriangleGeom : CustomGeom
{
public:
    TriangleGeom(vector<m2::PointF> &points, vector<size_t> triangles, dp::Color &color );
    void CreatePolys( TPolyFun callback );
private:
    vector<m2::PointF> m_points;
    vector<size_t> m_triangles;
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
public:
    using Geoms = vector<shared_ptr<CustomGeom>>;
    static CustomGeometries* GetInstance() {
        if (!s_inst){
            s_inst=new CustomGeometries();
        }
        return s_inst;
    }
    inline void SetGeomsXmlFile(string fileName){
        m_geomsXmlFile = fileName;
    }

    vector<shared_ptr<CustomGeom>> const GetGeometries(m2::RectD rectangle );
    void ReloadGeometries();

private:
    static CustomGeometries* s_inst;
    CustomGeometries();
    CustomGeometries( const CustomGeometries& ); // no copy constructor
    ~CustomGeometries () { }

    Geoms m_geoms;
    string m_geomsXmlFile = "";
};

} // end namespace df

#endif // CUSTOMGEOM_H
