############################################################################
# platform/nuttx/nuttx.mk
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

# Environment Checks ###########################################################

ifeq ($(OPT_LEVEL),)
$(warning The OPT_LEVEL variable is unset. Defaulting to 0.)
OPT_LEVEL = 0
endif

ifeq ($(OUTPUT_NAME),)
$(error "The OUTPUT_NAME variable must be set to the name of the desired \
         binary. Example: OUTPUT_NAME = my_nanoapp")
endif

# Variant-specific Support Source Files ########################################

SYS_SUPPORT_PATH = $(CHRE_PREFIX)/build/sys_support

# Host Toolchain ###############################################################

# The host toolchain is used to compile any programs for the compilation host
# in order to complete the build.

ifeq ($(CHRE_HOST_CC),)
CHRE_HOST_CC = g++
endif

# Makefile Includes ############################################################

# Common Includes
include $(CHRE_PREFIX)/build/tools_config.mk

# NanoPB Source Generation
include $(CHRE_PREFIX)/build/nanopb.mk

# TFLM Sources
include $(CHRE_PREFIX)/external/tflm/tflm.mk
