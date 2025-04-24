#pragma once

#include <chrono>


class Timer
{
public:
    Timer();

    void  start  (const char * msg);
    long double restart(const char * msg);
    long double stop();

    void setSteps(int steps);

protected:

    const char * m_msg;
    int m_steps;

    std::chrono::time_point<std::chrono::system_clock> time;
};
