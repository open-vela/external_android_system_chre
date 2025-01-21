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

#include <cstring>
#include "chre/platform/log.h"
#include "chre/platform/nuttx/wasm/chre_wasm_embed.h"

#include "wasm_export.h"

extern "C" bool wamr_custom_init(RuntimeInitArgs *init_args);

namespace chre {
static char globalHeapBuffer[CONFIG_CHRE_WASM_HEAP_BUFFER_SIZE] = { 0 };

bool WebAssemblyMicroRuntime::init()
{
    RuntimeInitArgs init_args;
    bool success = true;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));

    /* configure the memory allocator for the runtime */
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = globalHeapBuffer;
    init_args.mem_alloc_option.pool.heap_size = sizeof(globalHeapBuffer);

    /* initialize runtime environment with user configurations*/
    if (!wamr_custom_init(&init_args)) {
        LOGE("WAMR env initialization failed!");
        success = false;
    }

    return success;
}

void WebAssemblyMicroRuntime::deinit()
{
    wasm_runtime_destroy();
}

}  // namespace chre
