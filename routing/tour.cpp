#include "tour.hpp"
#include "geometry/point2d.hpp"
#include "geometry/mercator.hpp"

namespace routing
{

Tour::Tour()
{
    m_currentIndex=0;
    // TODO: load tour from file
    m_points.push_back( PointD( MercatorBounds::LonToX(12.123192), MercatorBounds::LatToY(47.796597) ) ); // Angerer Kurve (NO) / Einfahrt Mondi Inncoat
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121801), MercatorBounds::LatToY(47.797094) ) ); // Angerer (N)
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120500), MercatorBounds::LatToY(47.797566) ) ); // Kreuzung Angerer / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120908), MercatorBounds::LatToY(47.798967) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121029), MercatorBounds::LatToY(47.799725) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120902), MercatorBounds::LatToY(47.800527) ) ); // B15 Richtung Norden
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120513), MercatorBounds::LatToY(47.801354) ) ); // Kreuzung Eichenweg / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121119), MercatorBounds::LatToY(47.801512) ) ); // Eichenweg Richtung O
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121227), MercatorBounds::LatToY(47.801654) ) ); // Eichenweg Kurve
    m_points.push_back( PointD( MercatorBounds::LonToX(12.121151), MercatorBounds::LatToY(47.802211) ) ); // Eichenweg Richtung N
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120991), MercatorBounds::LatToY(47.802656) ) ); // Kreuzung Eichenweg / Eschenweg
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120404), MercatorBounds::LatToY(47.802446) ) ); // Eschenweg Richtung W
    m_points.push_back( PointD( MercatorBounds::LonToX(12.119868), MercatorBounds::LatToY(47.802172) ) ); // Kreuzung Eschenweg / B15
    m_points.push_back( PointD( MercatorBounds::LonToX(12.120449), MercatorBounds::LatToY(47.801371) ) ); // Kreuzung Eichenweg / B15
}

Tour::~Tour()
{
}

string Tour::GetName(){
    return "dummy tour";
}

PointD Tour::GetCurrentPoint(){
    return m_points.at(m_currentIndex);
}

vector<PointD> &Tour::GetAllPoints(){
    return m_points;
}

bool Tour::UpdateCurrentPosition( size_t index ){
    if (index>=m_points.size()) {
        return false;
    }
    m_currentIndex = index;
    return true;
}

} // end namespace routing

