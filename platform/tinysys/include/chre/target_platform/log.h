/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_TINYSYS_LOG_H_
#define CHRE_PLATFORM_TINYSYS_LOG_H_

#include "mt_printf.h"

// TODO(b/254292126): We should also print logs to logcat after hostlink
// implementation is ready.
#define LOGE(fmt, arg...) PRINTF_E(fmt, ##arg)
#define LOGW(fmt, arg...) PRINTF_W(fmt, ##arg)
#define LOGI(fmt, arg...) PRINTF_I(fmt, ##arg)
#define LOGD(fmt, arg...) PRINTF_D(fmt, ##arg)

#endif  // CHRE_PLATFORM_TINYSYS_LOG_H_
