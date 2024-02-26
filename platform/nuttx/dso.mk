############################################################################
# platform/nuttx/dso.mk
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

include $(APPDIR)/Make.defs
include $(CHRE_PREFIX)/build/variant/nuttx.mk

CXXEXT = .cc
MODULECC = $(CXX)

CXXMODULEFLAGS += -UCHRE_NANOAPP_INTERNAL
CXXMODULEFLAGS += -DCHRE_IS_NANOAPP_BUILD
CXXMODULEFLAGS += -I$(CHRE_PREFIX)/platform/shared/include/chre/platform/shared/libc
CXXMODULEFLAGS += $(COMMON_CFLAGS)
CXXMODULEFLAGS += $(NUTTX_CFLAGS)

ifneq ($(NUTTX_DSO_PROGNAME),)
  BIN = $(NUTTX_DSO_PROGNAME)
else
  BIN = $(shell basename $(shell pwd))
endif

SRCS = $(COMMON_SRCS)
SRCS += $(CHRE_PREFIX)/platform/nuttx/dynamic_nanoapp.cc

OBJS = $(SRCS:$(CXXEXT)=$(OBJEXT))

all: $(BIN)
.PHONY: all clean install
$(OBJS): %$(OBJEXT): %$(CXXEXT)
	@echo "MODULECC: $<"
	$(Q) $(MODULECC) -c $(CXXMODULEFLAGS) $< -o $@
$(BIN): $(OBJS)
	@echo "MODULELD: $(BIN)"
	$(Q) $(MODULELD) $(LDMODULEFLAGS) $(LDLIBPATH) -o $@ $(ARCHCRT0OBJ) $^ $(LDLIBS)
install: $(BIN)
	mkdir -p $(OUT_APP)
	cp -vf $(BIN) $(OUT_APP)
dso_clean:
	$(call DELFILE, $(BIN))
	$(call CLEAN)
