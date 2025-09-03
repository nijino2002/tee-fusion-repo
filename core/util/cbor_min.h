#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct { uint8_t* buf; size_t cap; size_t len; int err; } cbor_min_t;
static inline void cbor_init(cbor_min_t* c, uint8_t* buf, size_t cap){ c->buf=buf; c->cap=cap; c->len=0; c->err=0; }
void cbor_put_type(cbor_min_t* c, uint8_t major, uint64_t val);
void cbor_put_bytes(cbor_min_t* c, const void* p, size_t n);
void cbor_put_uint(cbor_min_t* c, uint64_t v);
void cbor_put_bool(cbor_min_t* c, int b);
void cbor_put_bstr(cbor_min_t* c, const void* p, size_t n);
void cbor_put_tstr(cbor_min_t* c, const char* s);
void cbor_open_map(cbor_min_t* c, uint64_t pairs);
void cbor_open_array(cbor_min_t* c, uint64_t items);
