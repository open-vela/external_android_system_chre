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

#ifndef CHRE_PLATFORM_NUTTX_PLATFORM_NANOAPP_BASE_H_
#define CHRE_PLATFORM_NUTTX_PLATFORM_NANOAPP_BASE_H_

#include <cstdint>
#include <string>

#include "chre/platform/shared/nanoapp_support_lib_dso.h"

namespace chre {

/**
 * Linux-specific nanoapp functionality.
 */
class PlatformNanoappBase {
 public:
  /**
   * Sets app info that will be used later when the app is loaded into the
   * system.
   *
   * @param appId The unique app identifier associated with this binary
   * @param appVersion An application-defined version number
   * @param appFilename The filename of the app that should be loaded from disk
   * @param targetApiVersion The target API version the nanoapp was compiled for
   *
   * @return true if the info was successfully stored
   */
  bool setAppInfo(uint64_t appId, uint32_t appVersion,
                  const std::string &appFilename, uint32_t targetApiVersion);
  /**
   * Associate this Nanoapp with a nanoapp included in a .so that is pre-loaded
   * onto the filesystem. Actually loading the .so into memory is done when
   * start() is called.
   *
   * @param filename The name of the .so file in /vendor/lib/dsp that holds this
   *        nanoapp. This string is not deep-copied, so the memory must remain
   *        valid for the lifetime of this Nanoapp instance.
   */
  void loadFromFile(const std::string &filename);

  /**
   * Associate this Nanoapp with a nanoapp included in elf that is pre-loaded
   * onto the filesystem. Actually loading the elf into memory is done before
   * start() is called.
   *
   * @param filename The name of the .so file in /vendor/lib/dsp that holds this
   *        nanoapp. This string is not deep-copied, so the memory must remain
   *        valid for the lifetime of this Nanoapp instance.
   */
  static bool setfilename(const std::string &filename);

  /**
   * Associate this Nanoapp instance with a nanoapp that is statically built
   * into the CHRE binary with the given app info structure.
   */
  void loadStatic(const struct chreNslNanoappInfo *appInfo);

  /**
   * @return true if the app's binary data is resident in memory, i.e. a
   *         previous call to loadFromBuffer() or loadStatic() was successful
   */
  bool isLoaded() const;
  /**
   * Reserves buffer space for a nanoapp's binary. This method should be called
   * before copyNanoappFragment is called.
   *
   * @param appId The unique app identifier associated with this binary
   * @param appVersion An application-defined version number
   * @param appFlags The flags provided by the app being loaded
   * @param appBinaryLen Size of appBinary, in bytes
   * @param targetApiVersion The target API version of the nanoapp
   *
   * @return true if the allocation was successful, false otherwise
   */
  bool reserveBuffer(uint64_t appId, uint32_t appVersion, uint32_t appFlags,
                     size_t appBinaryLen, uint32_t targetApiVersion);

  /**
   * Copies the (possibly fragmented) application binary data into the allocated
   * buffer, and updates the pointer to the next address to write into. The
   * application may be invalid - full checking and initialization happens just
   * before invoking start() nanoapp entry point.
   *
   * @param buffer The pointer to the buffer
   * @param bufferSize The size of the buffer in bytes
   *
   * @return true if the reserved buffer did not overflow, false otherwise
   */
  bool copyNanoappFragment(const void *buffer, size_t bufferSize);

  bool receiveComplete() const {
    return mAppBinaryLen > 0 && mAppBinaryLen == mBytesLoaded;
  }

 protected:
  //! The app ID we received in the metadata alongside the nanoapp binary. This
  //! is also included in (and checked against) mAppInfo.
  uint64_t mExpectedAppId;

  //! The dynamic shared object (DSO) handle returned by dlopen.
  void *mDsoHandle = nullptr;

  //! Pointer to the app info structure within this nanoapp
  const struct chreNslNanoappInfo *mAppInfo = nullptr;

  bool mIsStatic = false;

  //! If this is a pre-loaded, but non-static nanoapp (i.e. loaded from
  //! loadFromFile), this will be set to the filename string to pass to dlopen()
  std::string mFilename;

  //! If the nanoapp is send by host, for elf file,need to save it to filesystem
  static std::string mSavefilename;
  /**
   * The application-defined version number we received in the metadata
   * alongside the nanoapp binary. This is also included in (and checked
   * against) mAppInfo.
   */
  uint32_t mExpectedAppVersion = 0;
  //! The app target API version in the metadata alongside the nanoapp binary.
  uint32_t mExpectedTargetApiVersion = 0;

  //! Pointer containing the unstable ID section for this nanoapp
  const char *mAppUnstableId = nullptr;

  //! Use for save nanoappp to file
  FILE *mAppfd = NULL;

  size_t mAppBinaryLen = 0;

  //! The number of bytes of the binary that has been loaded so far.
  size_t mBytesLoaded = 0;

  /**
   * Calls through to openNanoappFromFile if the nanoapp was loaded from a
   * shared object or returns true if the nanoapp is static.
   *
   * @return true if the nanoapp was loaded successfully.
   */
  bool openNanoapp();

  /**
   * Calls dlopen on the app filename, and fetches and validates the app info
   * pointer. This will result in execution of any on-load handlers (e.g.
   * static global constructors) in the nanoapp.
   *
   * @return true if the app was opened successfully and the app info
   *         structure passed validation
   */
  bool openNanoappFromFile();

  /**
   * Releases the DSO handle if it was active, by calling dlclose(). This will
   * result in execution of any unload handlers in the nanoapp.
   */
  void closeNanoapp();
};

}  // namespace chre

#endif  // CHRE_PLATFORM_NUTTX_PLATFORM_NANOAPP_BASE_H_
