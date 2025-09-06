#pragma once
#include <stddef.h>
#include <stdint.h>
#include <tee_internal_api.h>
#include <utee_defines.h>

/* Reuse UUID from adapters header to keep CA/TA in sync */
#include "../../adapters/optee/optee_fusion_ta_uuid.h"

#define TA_UUID TA_OPTEE_FUSION_UUID

/* Basic TA properties: multi-session, keep-alive for demo */
#define TA_FLAGS        (TA_FLAG_MULTI_SESSION | TA_FLAG_INSTANCE_KEEP_ALIVE)
#define TA_STACK_SIZE   (8 * 1024)
#define TA_DATA_SIZE    (64 * 1024)

/* Optional: description */
#define TA_DESCRIPTION  "tee-fusion demo TA"
