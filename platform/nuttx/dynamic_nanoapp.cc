/*
 * Copyright (C) 2023 Xiaomi Corporation
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

#include <sys/types.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <syslog.h>

#include <nuttx/symtab.h>
#include <nuttx/lib/modlib.h>

#include "chre/util/nanoapp/log.h"
#include "chre_api/chre/nanoapp.h"
#include "chre_nsl_internal/platform/shared/nanoapp_support_lib_dso.h"
#include "chre_nsl_internal/util/macros.h"

#define LOG_TAG "[Module]"

extern "C" DLL_EXPORT const char _chreNanoappUnstableId[]
    __attribute__((section(".unstable_id"))) __attribute__((aligned(8))) =
        NANOAPP_UNSTABLE_ID;

extern "C" DLL_EXPORT const struct chreNslNanoappInfo _chreNslDsoNanoappInfo =
{
  CHRE_NSL_NANOAPP_INFO_MAGIC,
  CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION,
  NANOAPP_IS_SYSTEM_NANOAPP,
  0,
  0,
  0,
  CHRE_API_VERSION,
  NANOAPP_VENDOR_STRING,
  NANOAPP_NAME_STRING,
  NANOAPP_ID,
  NANOAPP_VERSION,
  {
    nanoappStart,
#if !defined(CHRE_NANOAPP_DISABLE_BACKCOMPAT) && defined(CHRE_NANOAPP_USES_GNSS)
    nanoappHandleEventCompat,
#else
    nanoappHandleEvent,
#endif
    nanoappEnd,
  },
  _chreNanoappUnstableId,
  0,
};

static const struct symtab_s g_chre_exports[4] =
{
  {
    "_chreNslDsoNanoappInfo", (const void *)&_chreNslDsoNanoappInfo,
  },
  {
    "nanoappEnd", (const void *)nanoappEnd,
  },
  {
    "nanoappHandleEvent", (const void *)nanoappHandleEvent,
  },
  {
    "nanoappStart", (const void *)nanoappStart,
  },
};

static int module_uninitialize(void *arg)
{
  LOGI("module_uninitialize: arg=%p\n", arg);
  return OK;
}

extern "C" int module_initialize(struct mod_info_s *modinfo)
{
  LOGI("module_initialize:\n");

  modinfo->uninitializer = module_uninitialize;
  modinfo->arg           = NULL;
  modinfo->exports       = g_chre_exports;
  modinfo->nexports      = 4;

  return OK;
}
