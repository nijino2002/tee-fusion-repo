#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../include/tee_fusion.h"
typedef struct {
  char hw_model[32]; char iso_class[16]; int debug; unsigned int sv;
  uint8_t sw_meas[48]; size_t sw_meas_len;
  uint8_t extra_meas[48]; size_t extra_meas_len;
  uint8_t* native_quote; size_t native_quote_len;
} claims_stash_t;
static claims_stash_t G;
void mapping_reset(){ memset(&G,0,sizeof(G)); }
void mapping_set_common(const char* hw,const char* iso,int dbg,unsigned int sv){ snprintf(G.hw_model,32,"%s",hw); snprintf(G.iso_class,16,"%s",iso); G.debug=dbg; G.sv=sv; }
void mapping_set_sw_measurement(const uint8_t* m,size_t n){ size_t c=n>sizeof(G.sw_meas)?sizeof(G.sw_meas):n; memcpy(G.sw_meas,m,c); G.sw_meas_len=c; }
void mapping_add_extra_measurement(const uint8_t* m,size_t n){ size_t c=n>sizeof(G.extra_meas)?sizeof(G.extra_meas):n; memcpy(G.extra_meas,m,c); G.extra_meas_len=c; }
void mapping_set_native_quote(const uint8_t* q,size_t n){ if(G.native_quote) { free(G.native_quote); G.native_quote=NULL; } if(!q||!n) return; G.native_quote=(uint8_t*)malloc(n); memcpy(G.native_quote,q,n); G.native_quote_len=n; }
const claims_stash_t* mapping_get(){ return &G; }
