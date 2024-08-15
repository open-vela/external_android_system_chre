include $(PATH_CHRE)/build/clean_build_template_args.mk

TARGET_NAME = nuttx_wasm32
TARGET_CFLAGS = -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=2048
TARGET_PLATFORM_ID = 0x476f6f676c000001
TARGET_CFLAGS += -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_1

# Enable conversion warnings for the simulator. Since this is a platform 100%
# within our control we expect that there will be no conversion issues. It would
# be nice to enable this globally in the tools_config.mk but some vendor header
# files do not compile cleanly with it.
TARGET_CFLAGS += -Wconversion

ifneq ($(filter $(TARGET_NAME)% all, $(MAKECMDGOALS)),)
include $(PATH_CHRE)/build/nanoapp/nuttx_wasm.mk

# wasm32 Environment Checks #######################################################

WASM32_TOOLS_PREFIX = ${WASI_SDK_PATH}/bin

# wasm32 Tools ####################################################################
TARGET_CC  = $(WASM32_TOOLS_PREFIX)/clang
TARGET_CPP_C = $(WASM32_TOOLS_PREFIX)/clang++
# wasm Compiler Flags ###########################################################

WASM32_CFLAGS += --target=wasm32-wasi
WASM32_CFLAGS += -O0
WASM32_CFLAGS += -z stack-size=4096
WASM32_CFLAGS += -Wl,--initial-memory=65536
WASM32_CFLAGS += -Wl,--strip-all,--no-entry
WASM32_CFLAGS += -nostdlib
WASM32_CFLAGS += --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot
WASM32_CFLAGS += -Wl,--export=nanoappStart
WASM32_CFLAGS += -Wl,--export=nanoappEnd
WASM32_CFLAGS += -Wl,--export=nanoappHandleEvent
WASM32_CFLAGS += -Wl,--export=getnanoappHandleEvent
WASM32_CFLAGS += -Wl,--export=getNanoappInfo
WASM32_CFLAGS += -Wl,--allow-undefined

COMMON_CXX_FLAGS += --std=c++17
COMMON_C_FLAGS += --std=c11

TARGET_CFLAGS += -g

# Enable position independence.
TARGET_CFLAGS += -fpic

# Disable double promotion warning for logging
TARGET_CFLAGS += -Wno-double-promotion

# Optimization Level ###########################################################

TARGET_CFLAGS += -O$(OPT_LEVEL)

# Source files.
CC_SRCS = $(filter %.cc, $(COMMON_SRCS) $(TARGET_VARIANT_SRCS))
CPP_SRCS = $(filter %.cpp, $(COMMON_SRCS) $(TARGET_VARIANT_SRCS))
C_SRCS = $(filter %.c, $(COMMON_SRCS) $(TARGET_VARIANT_SRCS))

# Object files.
CC_OBJS = $(patsubst %.cc, %.o, \
                           $(CC_SRCS))
CPP_OBJS = $(patsubst %.cpp, %.o, \
                            $(CPP_SRCS))
C_OBJS = $(patsubst %.c, %.o, \
                            $(C_SRCS))

# compile
C_COMPILE:
ifneq ($(C_SRCS),)
	$(TARGET_CC) -c --target=wasm32-wasi $(TARGET_CFLAGS) $(COMMON_C_FLAGS) $(COMMON_CFLAGS) $(C_SRCS)
endif

CPP_COMPILE:
ifneq ($(CC_SRCS)_$(CPP_SRCS),)
	$(TARGET_CPP_C) -c --target=wasm32-wasi $(TARGET_CFLAGS) $(COMMON_CXX_FLAGS) $(COMMON_CFLAGS) $(CC_SRCS) $(CPP_SRCS)
endif

OBJS = $(CC_OBJS) $(CPP_OBJS) $(C_OBJS)

$(TARGET_NAME): C_COMPILE CPP_COMPILE
	$(TARGET_CC) -o $(PROGNAME)$(WASM_SUFFIX) $(WASM32_CFLAGS) $(TARGET_CFLAGS) $(COMMON_CFLAGS) $(CC_OBJS) $(CPP_OBJS) $(C_OBJS)

endif