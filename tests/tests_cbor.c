#include <stdio.h>
#include "../core/util/cbor_min.h"
int main(){ uint8_t b[64]; cbor_min_t c; cbor_init(&c,b,sizeof(b)); cbor_open_map(&c,1); cbor_put_tstr(&c,"a"); cbor_put_uint(&c,1); printf("len=%zu\n", c.len); return c.err?1:0; }
