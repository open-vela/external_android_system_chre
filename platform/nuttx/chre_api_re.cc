/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <syslog.h>

#include "chre_api/chre/re.h"
#include "chre/util/macros.h"

DLL_EXPORT void chreLog(enum chreLogLevel level, const char *formatStr, ...) {
  va_list ap;

  va_start(ap, formatStr);
  switch (level) {
    case CHRE_LOG_ERROR:
      vsyslog(LOG_ERR, formatStr, ap);
      break;
    case CHRE_LOG_WARN:
      vsyslog(LOG_WARNING, formatStr, ap);
      break;
    case CHRE_LOG_INFO:
      vsyslog(LOG_INFO, formatStr, ap);
      break;
    case CHRE_LOG_DEBUG:
    default:
      vsyslog(LOG_DEBUG, formatStr, ap);
  }

  va_end(ap);
}
