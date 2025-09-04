#pragma once
/* Command IDs shared between CA and TA */
#define TA_CMD_GET_PSA_TOKEN   0x0001  /* out: memref token (PoC) */
#define TA_CMD_KEY_GEN         0x0002  /* create/rotate P-256 key inside TA */
#define TA_CMD_GET_PUBKEY_XY   0x0003  /* out: memref (X||Y) 32+32 bytes */
#define TA_CMD_KEY_SIGN        0x0004  /* in: msg -> out: DER ECDSA */
#define TA_CMD_RAND            0x0005  /* value.a=size -> out: random bytes */
