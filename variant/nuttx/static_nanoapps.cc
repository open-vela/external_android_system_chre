/*
 * Copyright (C) 2022 Xiaomi Corporation
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

#include "chre/core/static_nanoapps.h"
#include "chre/apps/apps.h"
#include "chre/util/macros.h"

namespace chre {

//! The default list of static nanoapps to load.
const StaticNanoappInitFunction kStaticNanoappList[] = {
#ifdef CHRE_LOAD_GNSS_WORLD
  initializeStaticNanoappGnssWorld,
#endif
#ifdef CHRE_LOAD_SENSOR_WORLD
  initializeStaticNanoappSensorWorld,
#endif
#ifdef CHRE_LOAD_WIFI_WORLD
  initializeStaticNanoappWifiWorld,
#endif
#ifdef CHRE_LOAD_WWAN_WORLD
  initializeStaticNanoappWwanWorld,
#endif
#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  initializeStaticNanoappAudioWorld,
#endif
#ifdef CHRE_BLE_SUPPORT_ENABLED
  initializeStaticNanoappBleWorld,
#endif
  initializeStaticNanoappDebugDumpWorld,
  initializeStaticNanoappHelloWorld,
  initializeStaticNanoappMessageWorld,
  initializeStaticNanoappSpammer,
  initializeStaticNanoappTimerWorld,
  initializeStaticNanoappUnloadTester,
};

//! The size of the default static nanoapp list.
const size_t kStaticNanoappCount = ARRAY_SIZE(kStaticNanoappList);

}  // namespace chre
