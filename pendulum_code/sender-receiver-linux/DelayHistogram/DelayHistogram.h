#include "../nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#ifndef DELAYHISTOGRAM_H
#define DELAYHISTOGRAM_H
typedef unsigned long Count;
typedef long Delay;

class DelayHistogram {
public:
  DelayHistogram(nlohmann::json hist_data, double reliability,
                 int consecutiveFrameLosses,
                 double durationMinutesBetweenFaults);
  void calc_packet_delay_budgets(double reliability);
  void update(double reliability, int consecutiveFrameLosses,
              double durationMinutesBetweenFaults);

  Delay random_sample();
  bool is_in_reliability_range(Delay delay);
  bool is_in_et_range(Delay delay);

  std::string printDelayBudgets();

  void test_reliability();

  Delay packetDelayBudgetMax;
  Delay packetDelayBudgetMin;

private:
  std::map<Delay, Count> histogram;
  std::map<Delay, double> probabilityComparisonMap;
  Count size, total_size;
  std::string name;

  int consecutiveFrameLosses;
  int frameLosses;
  double durationMinutesBetweenFaults;
  std::chrono::high_resolution_clock::time_point lastFault;
};

#endif // DELAYHISTOGRAM_H
