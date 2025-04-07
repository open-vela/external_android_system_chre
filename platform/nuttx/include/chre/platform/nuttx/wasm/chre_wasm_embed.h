/*
 * Copyright (C) 2024 Xiaomi Corperation
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

#ifndef CHRE_PLATFORM_NUTTX_CHRE_WASM_WASM_EMBED_H_
#define CHRE_PLATFORM_NUTTX_CHRE_WASM_WASM_EMBED_H_

namespace chre {
class WebAssemblyMicroRuntime {
 public:
  /**
   * Initialize the wasm environment, return 1 means
   * env initialization success.
   */
  static bool init();

  /**
   * Unregister the wasm API, and free wasm runtime.
   */
  static void deinit();
};
}

#endif  // CHRE_PLATFORM_NUTTX_CHRE_WASM_WASM_EMBED_H_
