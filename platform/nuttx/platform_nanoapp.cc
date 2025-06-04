/*
 * Copyright (C) 2024 Xiaomi Corporation
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

#include "chre/platform/platform_nanoapp.h"

#include <dlfcn.h>
#include <nuttx/symtab.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cinttypes>

#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/nanoapp_dso_util.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/version.h"
#include "chre/platform/memory.h"

extern const struct symtab_s CONFIG_CHRE_SYMTAB_ARRAYNAME[];
extern const int CONFIG_CHRE_NSYMBOLS_VAR;
#ifdef CHRE_WASM
#include "wasm_export.h"
#endif

namespace chre {
std::string PlatformNanoappBase::mSavefilename;

PlatformNanoapp::~PlatformNanoapp() { closeNanoapp(); }

#ifdef CHRE_WASM
bool PlatformNanoapp::start() {
  uint32_t argv[2] = { 0 };
  bool success = openNanoapp();
  if (success) {
    if (mIsWASM) {
      success = wasm_runtime_call_wasm(mWASMHandle.execEnv, mWASMHandle.nanoappStartFromWASM, 0, argv);
      if (!success) {
        LOGE("Wasm world Error Info: %s", wasm_runtime_get_exception(mWASMHandle.WASMModuleInstance));
      } else {
        success = static_cast<bool>(argv[0]);
      }
    } else {
      success = mAppInfo->entryPoints.start();
    }
  }
  return success;
}

void PlatformNanoapp::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  uint32_t argv[4];
  wasm_module_inst_t module_inst = nullptr;
  if (mWASMHandle.execEnv) {
    module_inst = get_module_inst(mWASMHandle.execEnv);
  }
  if (module_inst && mIsWASM) {
    argv[0] = senderInstanceId;
    argv[1] = eventType;
    argv[2] = addr_native_to_app((void *)eventData);
    if (!wasm_runtime_call_wasm(mWASMHandle.execEnv, mWASMHandle.nanoappHandleEventFromWASM, 3, argv)) {
      LOGE("Wasm world Error Info: %s", wasm_runtime_get_exception(mWASMHandle.WASMModuleInstance));
    }
  } else {
    mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
  }
}

void PlatformNanoapp::end() {
  uint32_t argv[2];
  if (mIsWASM) {
    if (!wasm_runtime_call_wasm(mWASMHandle.execEnv, mWASMHandle.nanoappEndFromWASM, 0, argv)) {
      LOGE("Wasm world Error Info: %s", wasm_runtime_get_exception(mWASMHandle.WASMModuleInstance));
    }
  } else {
    mAppInfo->entryPoints.end();
  }
  closeNanoapp();
}
#else
bool PlatformNanoapp::start() {
  return openNanoapp() && mAppInfo->entryPoints.start();
}

void PlatformNanoapp::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() {
  mAppInfo->entryPoints.end();
  closeNanoapp();
}
#endif

uint64_t PlatformNanoapp::getAppId() const {
  return (mAppInfo == nullptr) ? 0 : mAppInfo->appId;
}

uint32_t PlatformNanoapp::getAppVersion() const { return mAppInfo ? mAppInfo->appVersion : 0; }

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  return CHRE_API_VERSION;
}

const char *PlatformNanoapp::getAppName() const {
  return (mAppInfo != nullptr) ? mAppInfo->name : "Unknown";
}

bool PlatformNanoapp::supportsAppPermissions() const {
  return (mAppInfo != nullptr) ? (mAppInfo->structMinorVersion >=
                                  CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION_3)
                               : false;
}

uint32_t PlatformNanoapp::getAppPermissions() const {
  return (supportsAppPermissions())
             ? mAppInfo->appPermissions
             : static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE);
}

bool PlatformNanoapp::isSystemNanoapp() const {
  return (mAppInfo != nullptr && mAppInfo->isSystemNanoapp);
}

void PlatformNanoapp::logStateToBuffer(
    DebugDumpWrapper & /* debugDump */) const {}

void PlatformNanoappBase::loadFromFile(const std::string &filename) {
  CHRE_ASSERT(!isLoaded());
  mFilename = filename;
}

bool PlatformNanoappBase::setfilename(const std::string &filename) {
  if (access(CONFIG_CHRE_NANOAPP_SAVEPATH, F_OK) != 0) {
    if (mkdir(CONFIG_CHRE_NANOAPP_SAVEPATH, 0755) < 0) {
      return false;
    }
  }
  mSavefilename = std::string(CONFIG_CHRE_NANOAPP_SAVEPATH) + "/" + filename;
  return true;
}

bool PlatformNanoappBase::setAppInfo(uint64_t appId, uint32_t appVersion,
                                     const std::string &appFilename,
                                     uint32_t targetApiVersion) {
  CHRE_ASSERT(!isLoaded());
  if (access(appFilename.c_str(), F_OK) == 0) {
    mFilename = appFilename;
    mExpectedAppId = appId;
    mExpectedAppVersion = appVersion;
    mExpectedTargetApiVersion = targetApiVersion;
    return true;
  }
  return false;
}

void PlatformNanoappBase::loadStatic(const struct chreNslNanoappInfo *appInfo) {
  CHRE_ASSERT(!isLoaded());
  mIsStatic = true;
  mAppInfo = appInfo;
}

bool PlatformNanoappBase::isLoaded() const {
  return (mIsStatic || mDsoHandle != nullptr || !mFilename.empty() ||
          receiveComplete());
}

bool PlatformNanoappBase::openNanoapp() {
  bool success = false;

  if (mIsStatic) {
    success = true;
  } else if (!mFilename.empty()) {
    success = openNanoappFromFile();
  } else {
    CHRE_ASSERT(false);
  }

  return success;
}

bool PlatformNanoappBase::reserveBuffer(uint64_t appId, uint32_t appVersion,
                                        uint32_t appFlags, size_t appBinaryLen,
                                        uint32_t targetApiVersion) {
  CHRE_ASSERT(!isLoaded());
  if (mAppfd != NULL) {
    fclose(mAppfd);
    mAppfd = NULL;
  }
  if (mSavefilename.empty()) {
    mSavefilename =
        std::string(CONFIG_CHRE_NANOAPP_SAVEPATH) + "/" + std::to_string(appId);
  }
  mAppfd = fopen(mSavefilename.c_str(), "wb");
  if (mAppfd == NULL) {
    LOGE("open %s failed errno=%d", mSavefilename.c_str(), errno);
    return false;
  }
  mExpectedAppId = appId;
  mExpectedAppVersion = appVersion;
  mExpectedTargetApiVersion = targetApiVersion;
  mAppBinaryLen = appBinaryLen;
  return true;
}

bool PlatformNanoappBase::copyNanoappFragment(const void *buffer,
                                              size_t bufferLen) {
  CHRE_ASSERT(!isLoaded());

  if ((mBytesLoaded + bufferLen) > mAppBinaryLen) {
    LOGE("Overflow: cannot load %zu bytes to %zu/%zu nanoapp binary buffer",
         bufferLen, mBytesLoaded, mAppBinaryLen);
    return false;
  } else if (mAppfd) {
    size_t n = fwrite(buffer, 1, (int)bufferLen, mAppfd);
    if (n != bufferLen) {
      fclose(mAppfd);
      mAppfd = NULL;
      remove(mSavefilename.c_str());
      LOGE("fwrite size %zu != %zu errno=%d", n, bufferLen, errno);
      return false;
    } else {
      mBytesLoaded += bufferLen;
    }
    if (mAppBinaryLen == mBytesLoaded) {
      fclose(mAppfd);
      mAppfd = NULL;
      mFilename = mSavefilename;
      mSavefilename.clear();
    }
  }
  return true;
}

bool PlatformNanoappBase::openNanoappFromELFFile() {
  CHRE_ASSERT(!mFilename.empty());
  CHRE_ASSERT_LOG(mDsoHandle == nullptr, "Re-opening nanoapp");
  bool success = false;
  int ret;

  ret = dlsymtab(CONFIG_CHRE_SYMTAB_ARRAYNAME, CONFIG_CHRE_NSYMBOLS_VAR);
  if (ret != 0) {
    LOGE("Failed to set symbol table %s: %d, %s", mFilename.c_str(), ret,
         dlerror());
  }

  mDsoHandle = dlopen(mFilename.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (mDsoHandle == nullptr) {
    LOGE("Failed to load nanoapp from file %s: %s", mFilename.c_str(),
         dlerror());
  } else {
    mAppInfo = static_cast<const struct chreNslNanoappInfo *>(
        dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_INFO_SYMBOL_NAME));
    if (mAppInfo == nullptr) {
      LOGE("Failed to find app info symbol in %s: %s", mFilename.c_str(),
           dlerror());
    } else {
      // TODO(b/120778991): reenable this check after adding support for passing
      // in the .napp_header to the simulator
      // success = validateAppInfo(0 /* skip ID validation */, 0, mAppInfo);
      success = true;
      if (!success) {
        mAppInfo = nullptr;
      } else {
        LOGI("Nanoapp loaded: %s (0x%016" PRIx64 ") version 0x%" PRIx32
             " uimg %d system %d from file %s",
             mAppInfo->name, mAppInfo->appId, mAppInfo->appVersion,
             mAppInfo->isTcmNanoapp, mAppInfo->isSystemNanoapp,
             mFilename.c_str());
        if (mAppInfo->structMinorVersion >=
            CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION_3) {
          LOGI("Nanoapp permissions: 0x%" PRIx32, mAppInfo->appPermissions);
        }
      }
    }
  }

  return success;
}

bool PlatformNanoappBase::openNanoappFromFile() {
  CHRE_ASSERT(!mFilename.empty());
  bool success = false;
#ifdef CHRE_WASM
  success = openNanoappFromWASMFile();
#endif
  if (!success) {
    success = openNanoappFromELFFile();
  }
  return success;
}

#ifdef CHRE_WASM
bool PlatformNanoappBase::openNanoappFromWASMFile() {
  CHRE_ASSERT(!mFilename.empty());
  CHRE_ASSERT_LOG(mDsoHandle == nullptr && mWASMHandle.execEnv == nullptr, "Re-opening nanoapp");
  int fd;
  uint8_t *file_buf;
  char error_buf[128];
  struct stat stat_buf;
  struct chreNslNanoappInfo *app_info;

  if ((fd = open(mFilename.c_str(), O_RDONLY)) < 0) {
    LOGE("Open Wasm file failed!");
    goto fail0;
  }

  if (fstat(fd, &stat_buf) != 0) {
    LOGE("Stat Wasm file failed!");
    goto fail0;
  }

  mWASMHandle.WASMFileSize = stat_buf.st_size;
  mWASMHandle.WASMFileBuf = (uint8_t *)mmap(NULL, mWASMHandle.WASMFileSize, PROT_READ, MAP_SHARED | MAP_FILE, fd, 0);

  if (!mWASMHandle.WASMFileBuf || mWASMHandle.WASMFileBuf == (uint8_t*)MAP_FAILED) {
    LOGE("Load Wasm file into buffer failed!");
    goto fail0;
  }

  mWASMHandle.isXipFile = true;

  if (!wasm_runtime_is_xip_file(mWASMHandle.WASMFileBuf, mWASMHandle.WASMFileSize)) {
    mWASMHandle.isXipFile = false;
    file_buf = mWASMHandle.WASMFileBuf;
    mWASMHandle.WASMFileBuf = (uint8_t *)wasm_runtime_malloc(mWASMHandle.WASMFileSize);
    if (!mWASMHandle.WASMFileBuf) {
      LOGE("Allocate memory for Wasm file failed!");
      munmap(file_buf, mWASMHandle.WASMFileSize);
      goto fail0;
    }
    memcpy(mWASMHandle.WASMFileBuf, file_buf, mWASMHandle.WASMFileSize);
    munmap(file_buf, mWASMHandle.WASMFileSize);
    mWASMHandle.isXipFile = false;
  }

  if (!(mWASMHandle.WASMModule = wasm_runtime_load(mWASMHandle.WASMFileBuf, mWASMHandle.WASMFileSize, error_buf, sizeof(error_buf)))) {
    LOGE("Load Wasm module from buffer failed!");
    goto fail1;
  }

  if (!(mWASMHandle.WASMModuleInstance = wasm_runtime_instantiate(mWASMHandle.WASMModule, mWASMHandle.stackSize, mWASMHandle.heapSize, error_buf, sizeof(error_buf)))) {
    LOGE("Instantitate Wasm instance from module failed!");
    goto fail2;
  }

  if (!(mWASMHandle.execEnv = wasm_runtime_create_exec_env(mWASMHandle.WASMModuleInstance, mWASMHandle.heapSize))) {
    LOGE("Create Wasm execution environment from instance failed!");
    goto fail3;
  }

  mWASMHandle.nanoappStartFromWASM
      = wasm_runtime_lookup_function(mWASMHandle.WASMModuleInstance, "nanoappStart");
  mWASMHandle.nanoappHandleEventFromWASM
      = wasm_runtime_lookup_function(mWASMHandle.WASMModuleInstance, "nanoappHandleEvent");
  mWASMHandle.nanoappEndFromWASM
      = wasm_runtime_lookup_function(mWASMHandle.WASMModuleInstance, "nanoappEnd");

  if (!mWASMHandle.nanoappStartFromWASM || !mWASMHandle.nanoappHandleEventFromWASM
      || !mWASMHandle.nanoappEndFromWASM) {
    LOGE("Failed to lookup_function in wasm runtime");
    goto fail4;
  }

  app_info = (struct chreNslNanoappInfo *)memoryAlloc(sizeof(struct chreNslNanoappInfo));
  if (!app_info) {
    LOGE("Failed to allocate memory for app info");
    goto fail4;
  }

  mIsWASM = true;
  memset(app_info, 0, sizeof(struct chreNslNanoappInfo));
  // for wasm nanoapp, use the app infomation from the nanoapp header directly
  app_info->appId = mExpectedAppId;
  app_info->appVersion = mExpectedAppVersion;
  app_info->targetApiVersion = mExpectedTargetApiVersion;
  app_info->name = mFilename.c_str();
  mAppInfo = app_info;
  close(fd);
  return true;
fail4:
    mWASMHandle.nanoappStartFromWASM = nullptr;
    mWASMHandle.nanoappHandleEventFromWASM = nullptr;
    mWASMHandle.nanoappEndFromWASM = nullptr;
    wasm_runtime_destroy_exec_env(mWASMHandle.execEnv);
fail3:
    mWASMHandle.execEnv = nullptr;
    wasm_runtime_deinstantiate(mWASMHandle.WASMModuleInstance);
fail2:
    mWASMHandle.WASMModuleInstance = nullptr;
    wasm_runtime_unload(mWASMHandle.WASMModule);
fail1:
    mWASMHandle.WASMModule = nullptr;
    if (mWASMHandle.isXipFile) {
      munmap(mWASMHandle.WASMFileBuf, mWASMHandle.WASMFileSize);
    } else {
      wasm_runtime_free(mWASMHandle.WASMFileBuf);
    }
fail0:
    mWASMHandle.WASMFileBuf = nullptr;
    if (fd > 0) {
      close(fd);
    }
    return false;
}
#endif

void PlatformNanoappBase::closeNanoapp() {
  if (mDsoHandle != nullptr) {
#ifdef CHRE_WASM
    if (mIsWASM) {
      memoryFree((void *)mAppInfo);
    }
#endif
    mAppInfo = nullptr;
    if (dlclose(mDsoHandle) != 0) {
      LOGE("dlclose failed: %s", dlerror());
    }
    mDsoHandle = nullptr;
  }
#ifdef CHRE_WASM
  else if (mIsWASM) {
    wasm_runtime_destroy_exec_env(mWASMHandle.execEnv);
    wasm_runtime_deinstantiate(mWASMHandle.WASMModuleInstance);
    wasm_runtime_unload(mWASMHandle.WASMModule);
    if (mWASMHandle.WASMFileBuf) {
      if (mWASMHandle.isXipFile) {
        munmap(mWASMHandle.WASMFileBuf, mWASMHandle.WASMFileSize);
      } else {
        wasm_runtime_free(mWASMHandle.WASMFileBuf);
      }
    }

    mIsWASM = false;
    mWASMHandle.execEnv = nullptr;
    mWASMHandle.WASMModuleInstance = nullptr;
    mWASMHandle.WASMModule = nullptr;
    mWASMHandle.WASMFileBuf = nullptr;
  }
#endif
}

}  // namespace chre
