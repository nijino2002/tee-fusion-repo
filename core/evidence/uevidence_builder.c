#include <string.h>
#include <stdlib.h>
#include "../util/cbor_min.h"
#include "../../include/tee_fusion.h"
void mapping_reset(); void mapping_set_common(const char*,const char*,int,unsigned int);
void mapping_set_sw_measurement(const uint8_t*,size_t); void mapping_add_extra_measurement(const uint8_t*,size_t);
void mapping_set_native_quote(const uint8_t*,size_t);
typedef struct { char hw_model[32]; char iso_class[16]; int debug; unsigned int sv; uint8_t sw_meas[48]; size_t sw_meas_len; uint8_t extra_meas[48]; size_t extra_meas_len; uint8_t* native_quote; size_t native_quote_len; } claims_stash_t;
extern const claims_stash_t* mapping_get();
static uint8_t G_NONCE[64]; static size_t G_NONCE_LEN; static uint8_t G_PUB[192]; static size_t G_PUB_LEN;
const uint8_t* core_get_pubkey(size_t* plen){ if(plen)*plen=G_PUB_LEN; return G_PUB; }
void core_set_nonce_pubkey(const uint8_t* n,size_t nl,const uint8_t* p,size_t pl){ if(nl>sizeof(G_NONCE)) nl=sizeof(G_NONCE); memcpy(G_NONCE,n,nl); G_NONCE_LEN=nl; if(pl>sizeof(G_PUB)) pl=sizeof(G_PUB); memcpy(G_PUB,p,pl); G_PUB_LEN=pl; }
int core_build_uevidence(tee_buf_t* out){ if(!out) return TEE_EINVAL; const claims_stash_t* C=(const claims_stash_t*)mapping_get(); uint8_t tmp[16384]; cbor_min_t c; cbor_init(&c,tmp,sizeof(tmp)); int pairs=9+(C->extra_meas_len?1:0);
  cbor_open_map(&c,pairs);
  cbor_put_tstr(&c,"profile"); cbor_put_tstr(&c,"tee-fusion/1");
  cbor_put_tstr(&c,"hw.model"); cbor_put_tstr(&c, C->hw_model[0]?C->hw_model:"unknown");
  cbor_put_tstr(&c,"isolation.class"); cbor_put_tstr(&c, C->iso_class[0]?C->iso_class:"unknown");
  cbor_put_tstr(&c,"debug"); cbor_put_bool(&c, C->debug?1:0);
  cbor_put_tstr(&c,"security.version"); cbor_put_uint(&c, C->sv);
  cbor_put_tstr(&c,"sw.measurement"); cbor_put_bstr(&c, C->sw_meas, C->sw_meas_len?C->sw_meas_len:32);
  if(C->extra_meas_len){ cbor_put_tstr(&c,"sw.measurements"); cbor_put_type(&c,4,1); cbor_put_bstr(&c, C->extra_meas, C->extra_meas_len); }
  cbor_put_tstr(&c,"nonce"); cbor_put_bstr(&c, G_NONCE, G_NONCE_LEN);
  cbor_put_tstr(&c,"pubkey"); cbor_put_bstr(&c, G_PUB, G_PUB_LEN);
  cbor_put_tstr(&c,"native_quote"); cbor_put_bstr(&c, C->native_quote, C->native_quote_len);
  if(c.err) return TEE_EINTERNAL; uint8_t* outb=(uint8_t*)malloc(c.len); if(!outb) return TEE_ENOMEM; memcpy(outb,tmp,c.len); out->ptr=outb; out->len=c.len; return TEE_OK; }
