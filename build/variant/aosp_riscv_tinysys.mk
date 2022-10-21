#
# Google Reference CHRE Implementation for MTK riscv Tinysys
#
# TODO(b/254121302): We will rename this file once targets from multiple riscv
# ISAs are needed. At that time platform.mk needs to be updated too.
#

include $(CHRE_PREFIX)/build/clean_build_template_args.mk

TARGET_NAME = aosp_riscv_tinysys
TARGET_CFLAGS = $(TINYSYS_CFLAGS)
TARGET_VARIANT_SRCS = $(AOSP_RISCV_TINYSYS_SRCS)
TARGET_BIN_LDFLAGS = $(AOSP_RISCV_TINYSYS_BIN_LDFLAGS)
TARGET_SO_EARLY_LIBS = $(AOSP_RISCV_TINYSYS_EARLY_LIBS)
TARGET_SO_LATE_LIBS = $(AOSP_RISCV_TINYSYS_LATE_LIBS)
TARGET_PLATFORM_ID = 0x476f6f676c003000

# Macros #######################################################################

TARGET_CFLAGS += -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_7
# TODO(b/254121302): Needs to confirm with MTK about the max message size below
TARGET_CFLAGS += -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=2048
TARGET_CFLAGS += -D__riscv
TARGET_CFLAGS += -DP_MODE_0
TARGET_CFLAGS += -DMRV55
TARGET_CFLAGS += -D_LIBCPP_HAS_NO_LONG_LONG
TARGET_CFLAGS += -DCFG_AMP_CORE1_EN

# Compiling flags ##############################################################

TARGET_CFLAGS += -fpic
TARGET_CFLAGS += --target=riscv32-unknown-elf
TARGET_CFLAGS += -march=rv32imafcv
TARGET_CFLAGS += -mcpu=MRV55E03
# TODO(b/254303377): warning of conversions from string literal to 'char *'
TARGET_CFLAGS += -Wno-writable-strings

# Other makefiles ##############################################################

ifneq ($(filter $(TARGET_NAME)% all, $(MAKECMDGOALS)),)
include $(CHRE_PREFIX)/build/arch/riscv.mk
include $(CHRE_PREFIX)/build/build_template.mk
endif
