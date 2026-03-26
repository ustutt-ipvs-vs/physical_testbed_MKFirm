#include "DelayHistogram.h"

#include <iostream>

DelayHistogram::DelayHistogram(nlohmann::json hist_data, double reliability,
                               int consecutiveFrameLosses,
                               double durationMinutesBetweenFaults)
    : size(0), total_size(0), histogram({}), probabilityComparisonMap({}),
      name(hist_data["name"]), consecutiveFrameLosses(consecutiveFrameLosses),
      frameLosses(0),
      durationMinutesBetweenFaults(durationMinutesBetweenFaults),
      lastFault(std::chrono::high_resolution_clock::now()) {
  Delay lower_bound = 0;
  for (auto bin : hist_data["data"]) {
    if (!histogram.empty()) {
      // Ignore empty bins
      if (bin["count"].template get<Delay>() == 0) {
        continue;
      }
      histogram[lower_bound] = bin["count"].template get<Delay>();
      total_size += histogram[lower_bound];
    }
    if (!bin["upper_bound"].is_null()) {
      std::string ub = bin["upper_bound"].template get<std::string>();
      std::string delay_str = ub.substr(0, ub.find(" "));
      lower_bound = static_cast<Delay>(stod(delay_str) * 1e6);
      histogram[lower_bound] = 0;
    }
  }

  size = total_size;

  // Calculate packet delay budget interval
  calc_packet_delay_budgets(reliability);
  std::cout << "Histogram reliability: " << reliability << ", "
            << printDelayBudgets() << std::endl;

  // Create probability map
  double probabilityComparison = 0.0;
  for (auto bin : histogram) {
    if (bin.first < packetDelayBudgetMax) {
      size -= histogram[bin.first];
      continue;
    }

    double compareValue = probabilityComparison + ((double)bin.second / size);
    probabilityComparisonMap[bin.first] = compareValue;
    probabilityComparison = compareValue;
  }

  // Print histogram + probability compare values
  // for (auto bin : probabilityComparisonMap) {
  //  std::cout << "Delay: " << bin.first << ", Count: " << bin.second << ", ProbabilityCompare: " << probabilityComparisonMap[bin.first] << std::endl;
  // }
}

void DelayHistogram::test_reliability() {
  long packetCount = 0;
  long dropCount = 0;
  for (long i = 0; i < 10000000; i++) {
    packetCount++;
    if (!is_in_reliability_range(random_sample())) {
      dropCount++;
    }
  }
  std::cout << "Test results. Measured reliability: "
            << 1.0 - (double)dropCount / packetCount
            << ", dropCount: " << dropCount << std::endl;
}

void DelayHistogram::update(double reliability, int consecutiveFrameLosses,
	    double durationMinutesBetweenFaults) {
  calc_packet_delay_budgets(reliability);
  this->consecutiveFrameLosses = consecutiveFrameLosses;
  frameLosses = 0;
  this->durationMinutesBetweenFaults = durationMinutesBetweenFaults;
  lastFault = std::chrono::high_resolution_clock::now();

  probabilityComparisonMap.clear();
  size = total_size;
  double probabilityComparison = 0.0;
  for (auto bin : histogram) {
    if (bin.first < packetDelayBudgetMax) {
      size -= histogram[bin.first];
      continue;
    }

    double compareValue = probabilityComparison + ((double)bin.second / size);
    probabilityComparisonMap[bin.first] = compareValue;
    probabilityComparison = compareValue;
  }
}

void DelayHistogram::calc_packet_delay_budgets(double reliability) {
  enum RTIPolicy { minimize_interval, minimize_dmax };
  RTIPolicy POLICY = minimize_dmax;

  if (reliability == 0) {
    packetDelayBudgetMin = 0;
    packetDelayBudgetMax = 0;
    return;
  }

  if (POLICY == minimize_interval) {
    packetDelayBudgetMin = 0;
    packetDelayBudgetMax = std::numeric_limits<Delay>::max();
    for (auto min_it = histogram.begin(); min_it != histogram.end(); ++min_it) {
      Count c = 0;
      auto max_it = min_it;
      for (; max_it != histogram.end(); ++max_it) {
        if (c >= total_size * reliability) {
          if (max_it->first - min_it->first <
              packetDelayBudgetMax - packetDelayBudgetMin) {
            packetDelayBudgetMax = max_it->first;
            packetDelayBudgetMin = min_it->first;
          }
          break;
        }
        c += max_it->second;
      }
    }
    return;
  } else if (POLICY == minimize_dmax) {
    Count c = 0;
    Delay min = histogram.begin()->first;
    for (auto &bin : histogram) {
      if (c >= total_size * reliability) {
        packetDelayBudgetMax = bin.first;
        packetDelayBudgetMin = min;
        return;
      }
      c += bin.second;
    }
  }

  throw std::logic_error("invalid RTIPolicy");
}

std::string DelayHistogram::printDelayBudgets() {
  return "Packet delay budget min: " + std::to_string(packetDelayBudgetMin) +
         ", max: " + std::to_string(packetDelayBudgetMax);
}

Delay DelayHistogram::random_sample() {
  auto now = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double, std::ratio<60>> timeSinceLastFault =
      now - lastFault;

  if (timeSinceLastFault >
          static_cast<std::chrono::duration<double, std::ratio<60>>>(
              durationMinutesBetweenFaults) &&
      consecutiveFrameLosses > 0) {
    frameLosses += 1;
    lastFault = now;
  } else if (frameLosses > 0) {
    frameLosses += 1;
  }

  Delay delay = 0;
  if (frameLosses > 0) {
    // Always returns lower_bound of randomly selected histogram bin.
    // Sufficient because bin-size is only ~0.1 ms.
    double randomValue = rand() / (double)RAND_MAX;
    delay = packetDelayBudgetMax;

    for (auto bin : probabilityComparisonMap) {
      if (randomValue > bin.second) {
        delay = bin.first;
      } else {
        break;
      }
    }
  } else {
    delay = packetDelayBudgetMax - 1;
  }

  if (frameLosses == consecutiveFrameLosses)
    frameLosses = 0;

  return delay;
}

bool DelayHistogram::is_in_reliability_range(Delay delay) {
  if (delay >= packetDelayBudgetMin && delay < packetDelayBudgetMax) {
    return true;
  }
  return false;
}

bool DelayHistogram::is_in_et_range(Delay delay) {
  // TODO: Check if delay is small enough to still arrive in time with ET
  // traffic
  return true;
}
