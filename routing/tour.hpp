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

class Tour
{
    using TD = turns::TurnDirection;
    using TI = turns::TurnItem;
public:
    Tour();
    Tour( string const & filePath );
    ~Tour();
    string GetName(){
        return m_name;
    }
    void SetName(string name){
        m_name=name;
    }

    bool UpdateCurrentPosition(size_t index);

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
    void AddPoint( double lat, double lon ){
        m_points.emplace_back(MercatorBounds::LonToX(lat),MercatorBounds::LatToY(lon));
    }

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
};

}  // end namespace routing

#endif // TOUR_H
