#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "../../include/secure_sapi/sapi.h"

static TEE_ObjectHandle g_key = TEE_HANDLE_NULL;

static TEE_Result ensure_key(){
  if (g_key != TEE_HANDLE_NULL) return TEE_SUCCESS;
  TEE_Result res = TEE_AllocateTransientObject(TEE_TYPE_ECDSA_KEYPAIR, 256, &g_key);
  if (res != TEE_SUCCESS) return res;
  TEE_Attribute attr; TEE_InitValueAttribute(&attr, TEE_ATTR_ECC_CURVE, TEE_ECC_CURVE_NIST_P256, 0);
  return TEE_GenerateKey(g_key, 256, &attr, 1);
}

int sa_rand(void* buf, size_t len){ if(!buf||!len) return -1; TEE_GenerateRandom(buf,len); return 0; }
int sa_key_new_p256(void){ if (g_key!=TEE_HANDLE_NULL){ TEE_FreeTransientObject(g_key); g_key=TEE_HANDLE_NULL; } return ensure_key()==TEE_SUCCESS?0:-1; }

int sa_get_pubkey_xy(uint8_t out_xy[64], size_t* out_len){
  if(!out_len||!out_xy) return -1; TEE_Result res=ensure_key(); if(res!=TEE_SUCCESS) return -1;
  uint32_t xl=32, yl=32; if(*out_len<64){ *out_len=64; return -2; }
  res = TEE_GetObjectBufferAttribute(g_key, TEE_ATTR_ECC_PUBLIC_VALUE_X, out_xy, &xl); if(res!=TEE_SUCCESS) return -1;
  res = TEE_GetObjectBufferAttribute(g_key, TEE_ATTR_ECC_PUBLIC_VALUE_Y, out_xy+32, &yl); if(res!=TEE_SUCCESS) return -1;
  *out_len=64; return 0;
}

int sa_sign_p256_sha256(const void* msg, size_t msg_len, uint8_t* sig_der, size_t* sig_len){
  if(!msg||!sig_der||!sig_len) return -1; if(ensure_key()!=TEE_SUCCESS) return -1;
  TEE_Result res; TEE_OperationHandle dig=TEE_HANDLE_NULL, sig=TEE_HANDLE_NULL;
  uint8_t digest[32]; size_t dlen=sizeof(digest);
  res=TEE_AllocateOperation(&dig, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0); if(res!=TEE_SUCCESS) goto out;
  res=TEE_DigestDoFinal(dig, msg, msg_len, digest, &dlen); if(res!=TEE_SUCCESS) goto out;
  res=TEE_AllocateOperation(&sig, TEE_ALG_ECDSA_SHA256, TEE_MODE_SIGN, 256); if(res!=TEE_SUCCESS) goto out;
  res=TEE_SetOperationKey(sig, g_key); if(res!=TEE_SUCCESS) goto out;
  res=TEE_AsymmetricSignDigest(sig, NULL, 0, digest, dlen, sig_der, sig_len); if(res!=TEE_SUCCESS) goto out;
out:
  if(dig) TEE_FreeOperation(dig); if(sig) TEE_FreeOperation(sig);
  return res==TEE_SUCCESS?0:-1;
}

int sa_verify_p256_sha256(const void* msg, size_t msg_len, const uint8_t* sig_der, size_t sig_len){
  if(!msg||!sig_der) return -1; if(ensure_key()!=TEE_SUCCESS) return -1;
  TEE_Result res; TEE_OperationHandle dig=TEE_HANDLE_NULL, ver=TEE_HANDLE_NULL;
  uint8_t digest[32]; size_t dlen=sizeof(digest);
  res=TEE_AllocateOperation(&dig, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0); if(res!=TEE_SUCCESS) goto out;
  res=TEE_DigestDoFinal(dig, msg, msg_len, digest, &dlen); if(res!=TEE_SUCCESS) goto out;
  res=TEE_AllocateOperation(&ver, TEE_ALG_ECDSA_SHA256, TEE_MODE_VERIFY, 256); if(res!=TEE_SUCCESS) goto out;
  res=TEE_SetOperationKey(ver, g_key); if(res!=TEE_SUCCESS) goto out;
  res=TEE_AsymmetricVerifyDigest(ver, NULL, 0, digest, dlen, sig_der, sig_len);
out:
  if(dig) TEE_FreeOperation(dig); if(ver) TEE_FreeOperation(ver);
  return res==TEE_SUCCESS?0:-1;
}

int sa_get_token(uint8_t* out, size_t* inout_len){
  const char tok[]="OPTEE_PSA_TOKEN_POC"; size_t need=sizeof(tok)-1;
  if(!inout_len) return -1; if(!out||*inout_len<need){ *inout_len=need; return -2; }
  TEE_MemMove(out, tok, need); *inout_len=need; return 0;
}
