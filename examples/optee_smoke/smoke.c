#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "../../include/tee_fusion.h"

static int verify_sig_der_pub(const uint8_t* pub_der,size_t pub_len,const uint8_t* msg,size_t msg_len,const uint8_t* sig,size_t sig_len){
  int ok=0; const unsigned char* p=pub_der; EVP_PKEY* pkey=d2i_PUBKEY(NULL,&p,(long)pub_len); if(!pkey) return 0;
  EVP_MD_CTX* md=EVP_MD_CTX_create(); if(!md){ EVP_PKEY_free(pkey); return 0; }
  if(EVP_DigestVerifyInit(md,NULL,EVP_sha256(),NULL,pkey)<=0) goto out;
  if(EVP_DigestVerifyUpdate(md,msg,msg_len)<=0) goto out;
  if(EVP_DigestVerifyFinal(md,sig,sig_len)<=0) goto out;
  ok=1;
out:
  EVP_MD_CTX_destroy(md); EVP_PKEY_free(pkey); return ok;
}

static void hexdump(const char* title,const uint8_t* p,size_t n){
  printf("%s (%zu bytes): ", title, n);
  for(size_t i=0;i<n && i<64;i++) printf("%02x", p[i]);
  if(n>64) printf("...");
  printf("\n");
}

int main(void){
  OPENSSL_init_ssl(0, NULL);
  OPENSSL_init_crypto(0, NULL);

  tee_info_t info; if(tee_init(&(tee_init_opt_t){.app_id="optee-smoke"}, &info)!=TEE_OK){ fprintf(stderr,"tee_init failed\n"); return 1; }
  printf("Platform=%d caps=0x%llx abi=%u\n", (int)info.platform, (unsigned long long)info.caps, info.abi_version);

  /* Random test */
  uint8_t rnd[16]; if(tee_get_random(rnd,sizeof(rnd))!=TEE_OK){ fprintf(stderr,"rand failed\n"); return 1; }
  hexdump("rand", rnd, sizeof(rnd));

  /* Report/token test */
  tee_buf_t rpt={0}; if(tee_get_report(&rpt)==TEE_OK){ hexdump("report", (const uint8_t*)rpt.ptr, rpt.len); if(rpt.ptr) free(rpt.ptr); }

  /* Keygen/sign/verify */
  tee_attested_key_t ak; if(tee_key_generate(TEE_EC_P256,&ak)!=TEE_OK){ fprintf(stderr,"keygen failed\n"); return 1; }
  hexdump("pubkey-der", ak.pubkey, ak.pubkey_len);
  uint8_t msg[32]; memset(msg,0xab,sizeof msg);
  uint8_t sig[256]; size_t sl=sizeof(sig); if(tee_key_sign(&ak,msg,sizeof(msg),sig,&sl)!=TEE_OK){ fprintf(stderr,"sign failed\n"); return 1; }
  hexdump("sig-der", sig, sl);
  int v=verify_sig_der_pub(ak.pubkey,ak.pubkey_len,msg,sizeof(msg),sig,sl);
  printf("verify=%s\n", v?"ok":"FAIL"); if(!v) return 1;

  /* U-Evidence */
  uint8_t nonce[32]; memset(nonce,0xcd,sizeof nonce);
  if(tee_key_bind_into_evidence(&ak, nonce, sizeof(nonce))!=TEE_OK){ fprintf(stderr,"bind failed\n"); return 1; }
  tee_buf_t ue={0}; if(tee_get_u_evidence(&ue)==TEE_OK){ hexdump("u-evidence", (const uint8_t*)ue.ptr, ue.len); if(ue.ptr) free(ue.ptr); }

  /* OCall echo test */
  const char in[]="hello-ocall"; char out[32]={0}; size_t olen=sizeof(out);
  if(tee_ocall(0,in,strlen(in),out,&olen)==TEE_OK){ printf("ocall echo: %.*s\n", (int)olen, out); }

  printf("OP-TEE smoke test done.\n");
  return 0;
}
