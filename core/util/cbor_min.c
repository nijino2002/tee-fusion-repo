#include "cbor_min.h"
#include <string.h>
static void w8(cbor_min_t* c, uint8_t v){ if(c->len<c->cap) c->buf[c->len]=v; else c->err=1; c->len++; }
static void wN(cbor_min_t* c, const void* p, size_t n){ if(c->len+n<=c->cap) memcpy(c->buf+c->len,p,n); else c->err=1; c->len+=n; }
void cbor_put_type(cbor_min_t* c, uint8_t major, uint64_t val){
  if(c->err) return;
  if(val<=23) w8(c,(major<<5)|val);
  else if(val<=0xFF){ w8(c,(major<<5)|24); w8(c,(uint8_t)val); }
  else if(val<=0xFFFF){ w8(c,(major<<5)|25); uint8_t b[2]={(uint8_t)(val>>8),(uint8_t)val}; wN(c,b,2); }
  else if(val<=0xFFFFFFFFULL){ w8(c,(major<<5)|26); uint8_t b[4]={(uint8_t)(val>>24),(uint8_t)(val>>16),(uint8_t)(val>>8),(uint8_t)val}; wN(c,b,4); }
  else { w8(c,(major<<5)|27); uint8_t b[8]={ (uint8_t)(val>>56),(uint8_t)(val>>48),(uint8_t)(val>>40),(uint8_t)(val>>32),(uint8_t)(val>>24),(uint8_t)(val>>16),(uint8_t)(val>>8),(uint8_t)val}; wN(c,b,8); }
}
void cbor_put_bytes(cbor_min_t* c, const void* p, size_t n){ if(!c->err) wN(c,p,n); }
void cbor_put_uint(cbor_min_t* c, uint64_t v){ cbor_put_type(c,0,v); }
void cbor_put_bool(cbor_min_t* c, int b){ cbor_put_type(c,7, b?21:20); }
void cbor_put_bstr(cbor_min_t* c, const void* p, size_t n){ cbor_put_type(c,2,n); cbor_put_bytes(c,p,n); }
void cbor_put_tstr(cbor_min_t* c, const char* s){ size_t n=0; while(s&&s[n]) ++n; cbor_put_type(c,3,n); cbor_put_bytes(c,s,n); }
void cbor_open_map(cbor_min_t* c, uint64_t pairs){ cbor_put_type(c,5,pairs); }
void cbor_open_array(cbor_min_t* c, uint64_t items){ cbor_put_type(c,4,items); }
