/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOCATION_LBS_CONTEXTHUB_NANOAPPS_NEARBY_ADV_REPORT_CACHE_H_
#define LOCATION_LBS_CONTEXTHUB_NANOAPPS_NEARBY_ADV_REPORT_CACHE_H_

#ifdef NEARBY_PROFILE
#include <ash/profile.h>
#endif

#include "third_party/contexthub/chre/util/include/chre/util/dynamic_vector.h"

namespace nearby {

class AdvReportCache {
 public:
  // Constructs advertise report cache.
#ifdef NEARBY_PROFILE
  AdvReportCache() {
    ashProfileInit(
        &profile_data_, "[NEARBY_ADV_CACHE_PERF]", 1000 /* print_interval_ms */,
        false /* report_total_thread_cycles */, true /* printCsvFormat */);
  }
#else
  AdvReportCache() = default;
#endif

  // Deconstructs advertise report cache and releases all resources.
  ~AdvReportCache() {
    Clear();
  }

  // Releases all resources {cache element, heap memory} in cache.
  void Clear();

  // Adds advertise report to cache with deduplicating by
  // unique key which is {advertiser address and data}.
  // Among advertise report with the same key, latest one will be placed at the
  // same index of existing report in advertise reports cache.
  void Push(const chreBleAdvertisingReport &report);

  // Return advertise reports in cache.
  chre::DynamicVector<chreBleAdvertisingReport> &GetAdvReports() {
    return cache_reports_;
  }

  // Computes moving average with previous average (previous) and a current data
  // point (current). Returns the computed average.
  int8_t ComputeMovingAverage(int8_t previous, int8_t current) const {
    return static_cast<int8_t>(current * kMovingAverageWeight +
                               previous * (1 - kMovingAverageWeight));
  }

 private:
  // Weight for a current data point in moving average.
  static constexpr float kMovingAverageWeight = 0.3f;

  chre::DynamicVector<chreBleAdvertisingReport> cache_reports_;
#ifdef NEARBY_PROFILE
  ashProfileData profile_data_;
#endif
};

}  // namespace nearby

#endif  // LOCATION_LBS_CONTEXTHUB_NANOAPPS_NEARBY_ADV_REPORT_CACHE_H_
