#include <string.h>
#include <stdlib.h>
#include <openssl/rand.h>
#include "../../include/tee_fusion.h"
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
extern int  core_build_uevidence(tee_buf_t* out_cbor);
extern void core_set_nonce_pubkey(const uint8_t*, size_t, const uint8_t*, size_t);
static tee_adapter_vt g_vt; static tee_info_t g_info = { TEE_CLASS_UNKNOWN, 0, TEE_FUSION_ABI_VERSION };
tee_status_t tee_init(const tee_init_opt_t* opt, tee_info_t* out){ (void)opt; memset(&g_vt,0,sizeof(g_vt)); tee_register_active_adapter(&g_vt); if(g_vt.platform_id) g_info.platform=g_vt.platform_id(); g_info.caps=TEE_CAP_ATTESTATION|TEE_CAP_RA_TLS; if(out)*out=g_info; return TEE_OK; }
void         tee_exit(int code){ (void)code; }
tee_status_t tee_get_info(tee_info_t* out){ if(!out) return TEE_EINVAL; *out=g_info; return TEE_OK; }
tee_status_t tee_get_random(void* buf, size_t len){ if(!buf) return TEE_EINVAL; if(g_vt.rand_bytes) return g_vt.rand_bytes(buf,len); return RAND_bytes((unsigned char*)buf,(int)len)==1?TEE_OK:TEE_EINTERNAL; }
tee_status_t tee_trusted_time(uint64_t* ns){ if(!ns) return TEE_EINVAL; *ns=0; return TEE_ENOTSUP; }
tee_status_t tee_get_report(tee_buf_t* out){ if(!g_vt.get_report) return TEE_ENOTSUP; return g_vt.get_report(out); }
tee_status_t tee_get_u_evidence(tee_buf_t* out){ if(!g_vt.get_report||!g_vt.fill_platform_claims) return TEE_ENOTSUP; tee_buf_t rpt={0}; tee_status_t s=g_vt.get_report(&rpt); if(s!=TEE_OK) return s; s=g_vt.fill_platform_claims(rpt.ptr,rpt.len); free(rpt.ptr); if(s!=TEE_OK) return s; return core_build_uevidence(out); }
tee_status_t tee_key_generate(tee_key_algo_t algo, tee_attested_key_t* out_key){ if(!g_vt.key_new) return TEE_ENOTSUP; return g_vt.key_new(algo,out_key); }
tee_status_t tee_key_sign(const tee_attested_key_t* key,const void* m,size_t n,uint8_t* sig,size_t* sl){ (void)key; if(!g_vt.key_sign) return TEE_ENOTSUP; return g_vt.key_sign(m,n,sig,sl); }
tee_status_t tee_key_bind_into_evidence(const tee_attested_key_t* key,const uint8_t* nonce,size_t nlen){ (void)key; extern const uint8_t* core_get_pubkey(size_t*); size_t plen=0; const uint8_t* pub=core_get_pubkey(&plen); core_set_nonce_pubkey(nonce,nlen,pub,plen); return TEE_OK; }
tee_status_t tee_ocall(uint32_t op,const void* in,size_t ilen,void* out,size_t* olen){ if(!g_vt.ocall) return TEE_ENOTSUP; return g_vt.ocall(op,in,ilen,out,olen); }
