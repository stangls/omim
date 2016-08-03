#ifndef CUSTOMGEOM_H
#define CUSTOMGEOM_H

#include "drape/color.hpp"
#include "geometry/rect2d.hpp"
#include "base/logging.hpp"
#include "std/mutex.hpp"

using std::shared_ptr;

namespace df
{

using TPolyFun = function<void (m2::PointF,m2::PointF,m2::PointF)>;

class CustomGeom
{
public:

    dp::Color GetColor() const { return m_color; }
    m2::RectD GetBoundingBox() const { return m_outerRect; }
    virtual void CreatePolys(TPolyFun callback ) const = 0;
    string GetTitle() const { return m_title; }
    virtual m2::PointF GetCenter() const = 0;

protected:
    m2::RectD m_outerRect;
    dp::Color m_color;
    string m_title = "";
};

class ConvexGeom : CustomGeom
{
public:
    ConvexGeom(string title, vector<m2::PointF> &outerPoints, dp::Color &color );
    void CreatePolys(TPolyFun callback ) const;
    m2::PointF GetCenter() const;
private:
    vector<m2::PointF> m_outerPoints;
};

class TriangleGeom : CustomGeom
{
public:
    TriangleGeom( string title, vector<m2::PointF> &points, vector<size_t> triangles, dp::Color &color );
    void CreatePolys( TPolyFun callback ) const;
    m2::PointF GetCenter() const;
private:
    vector<m2::PointF> m_points;
    vector<size_t> m_triangles;
    m2::PointF m_center;
};

inline string DebugPrint(CustomGeom const & c)
{
  ostringstream out;
  out << "Name = " << c.GetTitle();
  out << "Color = ( " << DebugPrint(c.GetColor()) << " ) ";
  out << "BoundingBox = " << DebugPrint(c.GetBoundingBox());
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

    Geoms const GetGeometries(m2::RectD rectangle );
    void ReloadGeometries();

private:
    static CustomGeometries* s_inst;
    CustomGeometries();
    CustomGeometries( const CustomGeometries& ); // no copy constructor
    ~CustomGeometries () { }

    mutable mutex m_mutex;
    Geoms m_geoms; // when accessing lock via m_mutex !
    string m_geomsXmlFile = "";
};

} // end namespace df

#endif // CUSTOMGEOM_H
