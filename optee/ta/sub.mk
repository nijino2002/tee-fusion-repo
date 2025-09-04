# TA build description for OP-TEE TA Dev Kit

# UUID must match the CA side and user_ta_header_defines.h
user-ta-uuid := 7a9b3b24-3e2f-4d5f-912d-8b7c1355629a

# Include current dir and adapter headers
global-incdirs-y += .
global-incdirs-y += ../../adapters/optee

# Source files
srcs-y += optee_fusion_ta.c

