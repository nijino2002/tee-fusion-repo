#pragma once
#include <stddef.h>
#include <stdint.h>

int fs_init(void);
int fs_get_token(uint8_t* out, size_t* inout_len);
int fs_key_gen(void);
int fs_get_pubkey_xy(uint8_t out_xy[64], size_t* out_len);
int fs_sign(const void* msg, size_t msg_len, uint8_t* sig_der, size_t* sig_len);
int fs_verify(const void* msg, size_t msg_len, const uint8_t* sig_der, size_t sig_len);

