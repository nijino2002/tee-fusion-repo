#pragma once
#include <stdint.h>

/* Unified CA<->SA command IDs and minimal ABI for sign/verify demo */
enum {
  FUS_CMD_GET_TOKEN   = 0x0001,
  FUS_CMD_KEY_GEN     = 0x0002, /* generate or rotate P-256 key inside SA */
  FUS_CMD_GET_PUB_XY  = 0x0003, /* out: X||Y 32+32 bytes */
  FUS_CMD_SIGN        = 0x0004, /* in: msg -> out: DER ECDSA */
  FUS_CMD_RAND        = 0x0005, /* value.a=size -> out: random bytes */
  FUS_CMD_VERIFY      = 0x0006  /* in: msg, sig(DER) -> out: value.a=1(ok)/0(bad) */
};

