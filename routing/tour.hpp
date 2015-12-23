#ifndef TOURROUTER_H
#define TOURROUTER_H

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

    vector<PointD> &GetAllPoints(){
        return m_points;
    }
    pvec::iterator GetCurrentIt() {
        return m_points.begin()+m_currentIndex;
    }
    pvec::iterator GetEndIt() {
        return m_points.end();
    }

    vector<double>::iterator GetTimesCurrentIt(){
        return m_times.begin()+m_currentIndex;
    }
    vector<double>::iterator GetTimesEndIt(){
        return m_times.end();
    }

    vector<turns::TurnItem> &GetAllTurns(){
        return m_turns;
    }

    vector<turns::TurnItem>::iterator GetTurnsCurrentIt(){
        return m_turns.begin()+m_currentIndex;
    }
    vector<turns::TurnItem>::iterator GetTurnsEndIt(){
        return m_turns.end();
    }

protected:
    void CalculateTimes();

    string m_name;
    size_t m_currentIndex;
    pvec m_points;
    vector<double> m_times;
    vector<turns::TurnItem> m_turns;
};

}  // end namespace routing

#endif // TOURROUTER_H
