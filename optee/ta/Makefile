ARCH ?= aarch64
BINARY := optee_fusion_ta

# Expect TA_DEV_KIT_DIR to be provided by CMake or environment
ifeq (,$(TA_DEV_KIT_DIR))
$(error TA_DEV_KIT_DIR is not set. Point it to the OP-TEE TA dev kit export, e.g. optee_os/out/export-ta_$(ARCH))
endif

# Optional: toolchain prefix, e.g. aarch64-linux-gnu-
CROSS_COMPILE ?= aarch64-linux-gnu-

# Default log level can be overridden by environment
CFG_TEE_TA_LOG_LEVEL ?= 0

# Source layout
SRCS := optee_fusion_ta.c

# Include TA Dev Kit makefile
include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

