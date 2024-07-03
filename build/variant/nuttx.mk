############################################################################
# build/variant/nuttx.mk
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

ifeq ($(PATH_CHRE),)
PATH_CHRE = $(CHRE_PREFIX)
endif

include $(PATH_CHRE)/build/clean_build_template_args.mk

TARGET_NAME = nuttx
TARGET_CFLAGS = -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=$(CONFIG_CHRE_MESSAGE_TO_HOST_MAX_SIZE)
TARGET_VARIANT_SRCS = $(NUTTX_SRCS)
TARGET_BIN_LDFLAGS = $(XIAOMI_ARM_NUTTX_BIN_LDFLAGS)
TARGET_SO_EARLY_LIBS = $(XIAOMI_ARM_NUTTX_EARLY_LIBS)
TARGET_SO_LATE_LIBS = $(XIAOMI_ARM_NUTTX_LATE_LIBS)
TARGET_PLATFORM_ID = 0x476f6f676c000001
TARGET_CFLAGS += -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_1

TARGET_CFLAGS += $(NUTTX_CFLAGS)

# Add the target CFLAGS after the -Wconversion warning to allow targets to
# disable it.
TARGET_CFLAGS += $(XIAOMI_ARM_NUTTX_CFLAGS)

ifneq ($(filter $(TARGET_NAME)% all, $(MAKECMDGOALS)),)
ifneq ($(IS_NANOAPP_BUILD),)
else
# Instruct the build to link a final executable.
TARGET_BUILD_BIN = true
endif

endif
