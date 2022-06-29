/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <cstdlib>
#include <fstream>

#include "chre_host/daemon_base.h"
#include "chre_host/file_stream.h"
#include "chre_host/log.h"
#include "chre_host/napp_header.h"

#include <json/json.h>

#ifdef CHRE_DAEMON_METRIC_ENABLED
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

using ::aidl::android::frameworks::stats::IStats;
using ::aidl::android::frameworks::stats::VendorAtom;
using ::aidl::android::frameworks::stats::VendorAtomValue;
namespace PixelAtoms = ::android::hardware::google::pixel::PixelAtoms;
#endif  // CHRE_DAEMON_METRIC_ENABLED

// Aliased for consistency with the way these symbols are referenced in
// CHRE-side code
namespace fbs = ::chre::fbs;

namespace android {
namespace chre {

ChreDaemonBase::ChreDaemonBase() : mChreShutdownRequested(false) {
  mLogger.init();
}

void ChreDaemonBase::loadPreloadedNanoapps() {
  constexpr char kPreloadedNanoappsConfigPath[] =
      "/vendor/etc/chre/preloaded_nanoapps.json";
  std::ifstream configFileStream(kPreloadedNanoappsConfigPath);

  Json::CharReaderBuilder builder;
  Json::Value config;
  if (!configFileStream) {
    LOGE("Failed to open config file '%s': %d (%s)",
         kPreloadedNanoappsConfigPath, errno, strerror(errno));
  } else if (!Json::parseFromStream(builder, configFileStream, &config,
                                    /* errorMessage = */ nullptr)) {
    LOGE("Failed to parse nanoapp config file");
  } else if (!config.isMember("nanoapps") || !config.isMember("source_dir")) {
    LOGE("Malformed preloaded nanoapps config");
  } else {
    const Json::Value &directory = config["source_dir"];
    for (Json::ArrayIndex i = 0; i < config["nanoapps"].size(); i++) {
      const Json::Value &nanoapp = config["nanoapps"][i];
      loadPreloadedNanoapp(directory.asString(), nanoapp.asString(),
                           static_cast<uint32_t>(i));
    }
  }
}

void ChreDaemonBase::loadPreloadedNanoapp(const std::string &directory,
                                          const std::string &name,
                                          uint32_t transactionId) {
  std::vector<uint8_t> headerBuffer;

  std::string headerFile = directory + "/" + name + ".napp_header";

  // Only create the nanoapp filename as the CHRE framework will load from
  // within the directory its own binary resides in.
  std::string nanoappFilename = name + ".so";

  if (readFileContents(headerFile.c_str(), &headerBuffer) &&
      !loadNanoapp(headerBuffer, nanoappFilename, transactionId)) {
    LOGE("Failed to load nanoapp: '%s'", name.c_str());
  }
}

bool ChreDaemonBase::loadNanoapp(const std::vector<uint8_t> &header,
                                 const std::string &nanoappName,
                                 uint32_t transactionId) {
  bool success = false;
  if (header.size() != sizeof(NanoAppBinaryHeader)) {
    LOGE("Header size mismatch");
  } else {
    // The header blob contains the struct above.
    const auto *appHeader =
        reinterpret_cast<const NanoAppBinaryHeader *>(header.data());

    // Build the target API version from major and minor.
    uint32_t targetApiVersion = (appHeader->targetChreApiMajorVersion << 24) |
                                (appHeader->targetChreApiMinorVersion << 16);

    success = sendNanoappLoad(appHeader->appId, appHeader->appVersion,
                              targetApiVersion, nanoappName, transactionId);
  }

  return success;
}

bool ChreDaemonBase::sendTimeSyncWithRetry(size_t numRetries,
                                           useconds_t retryDelayUs,
                                           bool logOnError) {
  bool success = false;
  while (!success && (numRetries-- != 0)) {
    success = sendTimeSync(logOnError);
    if (!success) {
      usleep(retryDelayUs);
    }
  }
  return success;
}

void ChreDaemonBase::handleNanConfigurationRequest(
    const ::chre::fbs::NanConfigurationRequestT * /*request*/) {
  LOGE("NAN is unsupported on this platform");
}

#ifdef CHRE_DAEMON_METRIC_ENABLED
void ChreDaemonBase::handleMetricLog(const ::chre::fbs::MetricLogT *metricMsg) {
  const std::vector<int8_t> &encodedMetric = metricMsg->encoded_metric;

  switch (metricMsg->id) {
    case PixelAtoms::Atom::kChrePalOpenFailed: {
      PixelAtoms::ChrePalOpenFailed metric;
      if (!metric.ParseFromArray(encodedMetric.data(), encodedMetric.size())) {
        LOGE("Failed to parse metric data");
      } else {
        std::vector<VendorAtomValue> values(2);
        values[0].set<VendorAtomValue::intValue>(metric.pal());
        values[1].set<VendorAtomValue::intValue>(metric.type());
        const VendorAtom atom{
            .reverseDomainName = "",
            .atomId = PixelAtoms::Atom::kChrePalOpenFailed,
            .values{std::move(values)},
        };
        reportMetric(atom);
      }
      break;
    }
    case PixelAtoms::Atom::kChreEventQueueSnapshotReported: {
      PixelAtoms::ChreEventQueueSnapshotReported metric;
      if (!metric.ParseFromArray(encodedMetric.data(), encodedMetric.size())) {
        LOGE("Failed to parse metric data");
      } else {
        std::vector<VendorAtomValue> values(6);
        values[0].set<VendorAtomValue::intValue>(
            metric.snapshot_chre_get_time_ms());
        values[1].set<VendorAtomValue::intValue>(metric.max_event_queue_size());
        values[2].set<VendorAtomValue::intValue>(
            metric.mean_event_queue_size());
        values[3].set<VendorAtomValue::intValue>(metric.num_dropped_events());
        // Last two values are not currently populated and will be implemented
        // later. To avoid confusion of the interpretation, we use UINT32_MAX
        // as a placeholder value.
        values[4].set<VendorAtomValue::intValue>(
            UINT32_MAX);  // max_queue_delay_us
        values[5].set<VendorAtomValue::intValue>(
            UINT32_MAX);  // mean_queue_delay_us
        const VendorAtom atom{
            .reverseDomainName = "",
            .atomId = PixelAtoms::Atom::kChreEventQueueSnapshotReported,
            .values{std::move(values)},
        };
        reportMetric(atom);
      }
      break;
    }
    default: {
#ifdef CHRE_LOG_ATOM_EXTENSION_ENABLED
      handleVendorMetricLog(metricMsg);
#else
      LOGW("Unknown metric ID %" PRIu32, metricMsg->id);
#endif  // CHRE_LOG_ATOM_EXTENSION_ENABLED
    }
  }
}

void ChreDaemonBase::reportMetric(const VendorAtom &atom) {
  const std::string statsServiceName =
      std::string(IStats::descriptor).append("/default");
  if (!AServiceManager_isDeclared(statsServiceName.c_str())) {
    LOGE("Stats service is not declared.");
    return;
  }

  std::shared_ptr<IStats> stats_client = IStats::fromBinder(ndk::SpAIBinder(
      AServiceManager_waitForService(statsServiceName.c_str())));

  const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(atom);
  if (!ret.isOk()) {
    LOGE("Failed to report vendor atom");
  }
}
#endif  // CHRE_DAEMON_METRIC_ENABLED

}  // namespace chre
}  // namespace android
