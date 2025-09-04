#pragma once
/* Shared adapter vtable interface between core and platform adapters. */
#include <stddef.h>
#include <stdint.h>
#include "../include/tee_fusion.h"

typedef struct {
  tee_status_t (*get_report)(tee_buf_t*);
  tee_status_t (*fill_platform_claims)(const uint8_t*, size_t);
  tee_status_t (*key_new)(tee_key_algo_t, tee_attested_key_t*);
  tee_status_t (*key_sign)(const void*, size_t, uint8_t*, size_t*);
  tee_status_t (*rand_bytes)(void*, size_t);
  tee_status_t (*ocall)(uint32_t, const void*, size_t, void*, size_t*);
  tee_class_t  (*platform_id)(void);
} tee_adapter_vt;

/* Implemented by each adapter to register itself as the active backend. */
void tee_register_active_adapter(tee_adapter_vt* vt);

