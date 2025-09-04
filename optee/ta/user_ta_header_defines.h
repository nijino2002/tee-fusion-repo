#pragma once
#include <tee_api_defines.h>
#include "../../adapters/optee/optee_fusion_ta_uuid.h"
#define TA_UUID TA_OPTEE_FUSION_UUID
#define TA_FLAGS      (TA_FLAG_SINGLE_INSTANCE | TA_FLAG_MULTI_SESSION)
#define TA_STACK_SIZE (2 * 1024)
#define TA_DATA_SIZE  (32 * 1024)
