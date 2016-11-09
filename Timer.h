#pragma once
#include <SDL2\SDL.h>
#include <string>
class Timer
{
#define NUM_SAMPLES 1000
public:
  Timer()
  {
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
      times[i] = -1.0;
    }
    freq = SDL_GetPerformanceFrequency();
  }
  void start()
  {
    stopped = false;
    begin = SDL_GetPerformanceCounter();
  }
  void stop()
  {
    end = SDL_GetPerformanceCounter();
    stopped = true;
    times[current_index] = get_last();
    ++current_index;
    if (current_index > NUM_SAMPLES - 1)
      current_index = 0;
  }
  double get_last()
  {
    if (!stopped)
    {
      return -1;
    }
    return (double)(end - begin) / freq;
  }
  double moving_average()
  {
    double sum = 0;
    int count = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
      if (times[i] != -1.0)
      {
        sum += times[i];
        count += 1;
      }
    }
    return sum / count;
  }
  int count_samples()
  {
    int count = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
      if (times[i] != -1.0)
        ++count;
    }
    return count;
  }
  double longest()
  {
    double longest = 0.0;
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
      double time = times[i];
      if (time != -1.0 && time > longest)
      {
        longest = time;
      }
    }
    return longest;
  }
  double shortest()
  {
    double shortest = times[0];
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
      double time = times[i];
      if (time != -1.0 && time < shortest)
      {
        shortest = time;
      }
    }
    return shortest;
  }
  double jitter() { return longest() - shortest(); }
  std::string string_report()
  {
    std::string s = "\n";
    s += "Last          : " + std::to_string(get_last()) + "\n";
    s += "Average       : " + std::to_string(moving_average()) + "\n";
    s += "Max           : " + std::to_string(longest()) + "\n";
    s += "Min           : " + std::to_string(shortest()) + "\n";
    s += "Jitter        : " + std::to_string(jitter()) + "\n";
    s += "Samples:      : " + std::to_string(count_samples()) + "\n";
    return s;
  }

private:
  Uint64 freq;
  Uint64 begin;
  Uint64 end;
  bool stopped = true;

  int current_index = 0;
  double times[NUM_SAMPLES];
};