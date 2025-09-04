# OP-TEE TA build skeleton for qemu-v8
# Set before make:
#   export TA_DEV_KIT_DIR=/path/to/optee_os/out/arm/export-ta_arm64
#   export CROSS_COMPILE=aarch64-linux-gnu-
include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk
BINARY=7a9b3b24-3e2f-4d5f-912d-8b7c1355629a
SRC := optee_fusion_ta.c
CPPFLAGS += -I. -I../../adapters/optee
