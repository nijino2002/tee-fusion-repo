#pragma once
#include <stddef.h>
#include <stdint.h>

/* Secure-side abstract API (S-API): implemented per-platform (OP-TEE / TDX / Keystone) */

int sa_rand(void* buf, size_t len);
int sa_key_new_p256(void); /* rotate/create ephemeral P-256 key */
int sa_get_pubkey_xy(uint8_t out_xy[64], size_t* out_len);
int sa_sign_p256_sha256(const void* msg, size_t msg_len, uint8_t* sig_der, size_t* sig_len);
int sa_verify_p256_sha256(const void* msg, size_t msg_len, const uint8_t* sig_der, size_t sig_len);
int sa_get_token(uint8_t* out, size_t* inout_len); /* optional, may return 0 with empty */

