#include "../include/secure_sapi/sapi.h"
#include "fusion_secure.h"

int fs_init(void){ return 0; }
int fs_get_token(uint8_t* out, size_t* inout_len){ return sa_get_token(out, inout_len); }
int fs_key_gen(void){ return sa_key_new_p256(); }
int fs_get_pubkey_xy(uint8_t out_xy[64], size_t* out_len){ return sa_get_pubkey_xy(out_xy, out_len); }
int fs_sign(const void* msg, size_t msg_len, uint8_t* sig_der, size_t* sig_len){ return sa_sign_p256_sha256(msg,msg_len,sig_der,sig_len); }
int fs_verify(const void* msg, size_t msg_len, const uint8_t* sig_der, size_t sig_len){ return sa_verify_p256_sha256(msg,msg_len,sig_der,sig_len); }

