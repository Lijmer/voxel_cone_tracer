#pragma once

#include <chrono>

class Timer
{
  typedef std::chrono::steady_clock Clock;

public:

  void Start() { startTime = Clock::now(); }
  float IntervalMS()
  {
    using namespace std::chrono;
    const microseconds us =
      duration_cast<microseconds>(Clock::now() - startTime);
    return (float)us.count() / 1000.0f;
  }

private:
  Clock::time_point startTime;
};
