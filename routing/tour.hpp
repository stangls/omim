#ifndef TOUR_H
#define TOUR_H

#include "route.hpp"

#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

namespace routing
{

typedef m2::PointD PointD;
typedef vector<PointD> pvec;

class Poi
{
    public:
        Poi(const string & message, double lat, double lon, size_t earliestIndex):
            m_message(message),
            m_pos(MercatorBounds::LonToX(lat),MercatorBounds::LatToY(lon))
        {
            m_earliestIndex = earliestIndex;
        }
        PointD GetPos() const {
            return m_pos;
        }
        string GetMessage() const {
            return m_message;
        }
        size_t GetEarliestIndex() const {
            return m_earliestIndex;
        }

    private:
        PointD m_pos;
        string m_message;
        size_t m_earliestIndex;
};

typedef vector<Poi> poivec;

class Tour
{
    using TD = turns::TurnDirection;
    using TI = turns::TurnItem;
public:
    using TPoiCallback = function<void(Poi const &)>;
    Tour();
    Tour( string const & filePath, TPoiCallback const & poiVisited );
    ~Tour();
    string GetName(){
        return m_name;
    }
    void SetName(string name){
        m_name=name;
    }

    // updates the current position by scrolling to it
    bool UpdateCurrentPosition(size_t index);
    // updates the current position by jumping to it (ignoring intermediate points, POIs and other stuff)
    bool JumpCurrentPosition(size_t index);

    size_t GetCurrentIndex() {
        return m_currentIndex;
    }

    const vector<PointD> &GetAllPoints(){
        return m_points;
    }
    pvec::iterator GetCurrentIt() {
        return m_points.begin()+m_currentIndex;
    }
    pvec::iterator GetEndIt() {
        return m_points.end();
    }
    void AddPoint( double lat, double lon );

    void AddPoi(const string & message, double lat, double lon);

    vector<double>::iterator GetTimesCurrentIt(){
        return m_times.begin()+m_currentIndex;
    }
    vector<double>::iterator GetTimesEndIt(){
        return m_times.end();
    }

    const vector<TI> &GetAllTurns(){
        return m_turns;
    }
    vector<TI>::iterator GetTurnsCurrentIt(){
        auto it = m_turns.begin();
        while (it!=m_turns.end()){
            if ((*it).m_index>=m_currentIndex)
                break;
            it++;
        }
        return it;
    }
    vector<TI>::iterator GetTurnsEndIt(){
        return m_turns.end();
    }
    void AddTurn( TI turnItem ){
        m_turns.push_back(turnItem);
    }

protected:
    void CalculateTimes();

    string m_name;
    size_t m_currentIndex;
    pvec m_points;
    vector<double> m_times;
    vector<TI> m_turns;
    vector<Poi> m_pois;
    size_t m_nextPoiIndex = 0;
    TPoiCallback m_poiVisitedCallback;

    const double MIN_POINT_DIST = 5; // meters
    const double MAX_POI_DIST = 25; // meters
};

}  // end namespace routing

#endif // TOUR_H
