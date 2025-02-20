
#include "Timer.h"

#include <iostream>


Timer::Timer() : m_msg(nullptr), m_steps(1)
{
}

void Timer::start(const char * msg)
{
    m_msg = msg;
    time = std::chrono::system_clock::now();
}

void Timer::setSteps(int steps)
{
    m_steps = steps;
}

long double Timer::stop()
{
    auto delta = std::chrono::system_clock::now() - time;

    long double us = std::chrono::duration_cast<std::chrono::duration<long double, std::micro>>(delta / m_steps).count();

    std::cout << m_msg << ": " << us << " us" << std::endl;

    return us;
}

long double Timer::restart(const char * msg)
{
    long double us = stop();
    start(msg);

    return us;
}
