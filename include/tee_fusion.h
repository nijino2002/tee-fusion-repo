#pragma once
#include <stddef.h>
#include <stdint.h>
#include "tee_fusion_err.h"
#include "tee_fusion_version.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t tee_caps_t;
enum { TEE_CAP_ATTESTATION=(1ull<<0), TEE_CAP_RA_TLS=(1ull<<5) };
typedef enum { TEE_CLASS_UNKNOWN=0, TEE_CLASS_TDX=2, TEE_CLASS_TRUSTZONE=4, TEE_CLASS_KEYSTONE=6 } tee_class_t;
typedef struct { const char* app_id; const char* manifest_json; } tee_init_opt_t;
typedef struct { uint8_t* ptr; size_t len; } tee_buf_t;
typedef struct { tee_class_t platform; tee_caps_t caps; uint32_t abi_version; } tee_info_t;
typedef enum { TEE_EC_P256=1 } tee_key_algo_t;
typedef struct { tee_key_algo_t algo; uint8_t pubkey[192]; size_t pubkey_len; } tee_attested_key_t;
tee_status_t tee_init(const tee_init_opt_t* opt, tee_info_t* out_info);
void         tee_exit(int code);
tee_status_t tee_get_info(tee_info_t* out_info);
tee_status_t tee_get_random(void* buf, size_t len);
tee_status_t tee_trusted_time(uint64_t* ns);
tee_status_t tee_get_report(tee_buf_t* out_report);
tee_status_t tee_get_u_evidence(tee_buf_t* out_cbor);
tee_status_t tee_key_generate(tee_key_algo_t algo, tee_attested_key_t* out_key);
tee_status_t tee_key_sign(const tee_attested_key_t* key, const void* msg, size_t msg_len, uint8_t* sig, size_t* sig_len);
tee_status_t tee_key_bind_into_evidence(const tee_attested_key_t* key, const uint8_t* nonce, size_t nonce_len);
tee_status_t tee_ocall(uint32_t op, const void* in, size_t ilen, void* out, size_t* olen);
#ifdef __cplusplus
}
#endif
