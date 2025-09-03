#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/rand.h>
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
extern void tee_register_active_adapter(tee_adapter_vt* vt);
/* mapping glue */
void mapping_reset();
void mapping_set_common(const char*, const char*, int, unsigned int);
void mapping_set_sw_measurement(const uint8_t*, size_t);
void mapping_add_extra_measurement(const uint8_t*, size_t);
void mapping_set_native_quote(const uint8_t*, size_t);
/* key helpers */
static EVP_PKEY* g_priv=NULL; static unsigned char g_pub_der[192]; static size_t g_pub_der_len=0;
static int ensure_key(){ if(g_priv) return 1; int ok=0; EVP_PKEY_CTX* c=EVP_PKEY_CTX_new_id(EVP_PKEY_EC,NULL); if(!c) return 0;
  if(EVP_PKEY_keygen_init(c)<=0) goto done;
  if(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(c,NID_X9_62_prime256v1)<=0) goto done;
  if(EVP_PKEY_keygen(c,&g_priv)<=0) goto done;
  int len=i2d_PUBKEY(g_priv,NULL); if(len<=0 || (size_t)len>sizeof(g_pub_der)) goto done;
  unsigned char* p=g_pub_der; len=i2d_PUBKEY(g_priv,&p); g_pub_der_len=(size_t)len; ok=1;
done: EVP_PKEY_CTX_free(c); return ok; }
static tee_status_t key_new(tee_key_algo_t a, tee_attested_key_t* out){ if(a!=TEE_EC_P256||!out) return TEE_EINVAL; if(!ensure_key()) return TEE_EINTERNAL; out->algo=TEE_EC_P256; if(g_pub_der_len>sizeof(out->pubkey)) return TEE_ENOMEM; memcpy(out->pubkey,g_pub_der,g_pub_der_len); out->pubkey_len=g_pub_der_len; return TEE_OK; }
static tee_status_t key_sign(const void* m,size_t n,uint8_t* sig,size_t* sl){ if(!g_priv||!sig||!sl) return TEE_EINVAL; EVP_MD_CTX* md=EVP_MD_CTX_create(); int ok=0;
  if(EVP_DigestSignInit(md,NULL,EVP_sha256(),NULL,g_priv)<=0) goto done;
  if(EVP_DigestSignUpdate(md,m,n)<=0) goto done;
  size_t need=0; if(EVP_DigestSignFinal(md,NULL,&need)<=0) goto done; if(*sl<need){*sl=need; EVP_MD_CTX_destroy(md); return TEE_ENOMEM;}
  if(EVP_DigestSignFinal(md,sig,sl)<=0) goto done; ok=1;
done: EVP_MD_CTX_destroy(md); return ok?TEE_OK:TEE_EINTERNAL; }
static tee_status_t rnd(void* b,size_t l){ return RAND_bytes((unsigned char*)b,(int)l)==1?TEE_OK:TEE_EINTERNAL; }
static tee_status_t ocall_echo(uint32_t op,const void* in,size_t ilen,void* out,size_t* olen){ (void)op; if(!out||!olen) return TEE_EINVAL; size_t n=ilen<*olen?ilen:*olen; memcpy(out,in,n); *olen=n; return TEE_OK; }

static tee_status_t get_report(tee_buf_t* out){ if(!out) return TEE_EINVAL; const char* p=getenv("OPTEE_TOKEN_FILE"); if(!p){ out->ptr=NULL; out->len=0; return TEE_OK; }
  FILE* f=fopen(p,"rb"); if(!f) return TEE_EINTERNAL; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  uint8_t* buf=(uint8_t*)malloc((size_t)n); fread(buf,1,(size_t)n,f); fclose(f); out->ptr=buf; out->len=(size_t)n; return TEE_OK; }
static tee_status_t fill(const uint8_t* tok,size_t n){ mapping_reset(); mapping_set_common("ARM-OPTEE","secure-world",0,1); uint8_t m[32]={0}; mapping_set_sw_measurement(m,32); if(tok&&n) mapping_set_native_quote(tok,n); return TEE_OK; }
static tee_class_t pid(){ return TEE_CLASS_TRUSTZONE; }
void tee_register_active_adapter(tee_adapter_vt* vt){ vt->get_report=get_report; vt->fill_platform_claims=fill; vt->key_new=key_new; vt->key_sign=key_sign; vt->rand_bytes=rnd; vt->ocall=ocall_echo; vt->platform_id=pid; }
