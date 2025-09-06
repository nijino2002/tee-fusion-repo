#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <tee_client_api.h>
#include "../../adapters/optee/optee_fusion_ta_uuid.h"
#include "../../include/fusion_abi.h"

static void hexdump(const char* title,const uint8_t* p,size_t n){
  printf("%s (%zu bytes): ", title, n);
  for(size_t i=0;i<n && i<64;i++) printf("%02x", p[i]);
  if(n>64) printf("...");
  printf("\n");
}

int main(void){
  TEEC_Context ctx; TEEC_Session sess; TEEC_Result r; TEEC_UUID uuid = TA_OPTEE_FUSION_UUID;
  r = TEEC_InitializeContext(NULL, &ctx);
  if(r!=TEEC_SUCCESS){ printf("TEEC_InitializeContext failed: 0x%x\n", r); return 1; }
  r = TEEC_OpenSession(&ctx,&sess,&uuid,TEEC_LOGIN_PUBLIC,NULL,NULL,NULL);
  if(r!=TEEC_SUCCESS){ printf("TEEC_OpenSession failed: 0x%x\n", r); TEEC_FinalizeContext(&ctx); return 1; }

  // RAND
  TEEC_Operation op; memset(&op,0,sizeof(op)); uint8_t rnd[16];
  op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
  op.params[0].value.a = sizeof(rnd); op.params[1].tmpref.buffer = rnd; op.params[1].tmpref.size = sizeof(rnd);
  r = TEEC_InvokeCommand(&sess, FUS_CMD_RAND, &op, NULL);
  if(r==TEEC_SUCCESS) hexdump("rand", rnd, sizeof(rnd)); else printf("RAND failed: 0x%x\n", r);

  // GET TOKEN (PSA token PoC)
  memset(&op,0,sizeof(op)); uint8_t tok[512];
  op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
  op.params[0].tmpref.buffer = tok; op.params[0].tmpref.size = sizeof(tok);
  r = TEEC_InvokeCommand(&sess, FUS_CMD_GET_TOKEN, &op, NULL);
  if(r==TEEC_SUCCESS) hexdump("token", tok, op.params[0].tmpref.size); else printf("GET_PSA_TOKEN failed: 0x%x\n", r);

  // KEY GEN
  memset(&op,0,sizeof(op)); op.paramTypes=TEEC_PARAM_TYPES(TEEC_NONE,TEEC_NONE,TEEC_NONE,TEEC_NONE);
  r = TEEC_InvokeCommand(&sess, FUS_CMD_KEY_GEN, &op, NULL);
  if(r!=TEEC_SUCCESS) printf("KEY_GEN failed: 0x%x\n", r);
  // PUBKEY X||Y
  memset(&op,0,sizeof(op)); uint8_t xy[64];
  op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,TEEC_NONE,TEEC_NONE,TEEC_NONE);
  op.params[0].tmpref.buffer=xy; op.params[0].tmpref.size=sizeof(xy);
  r = TEEC_InvokeCommand(&sess, FUS_CMD_GET_PUB_XY, &op, NULL);
  if(r==TEEC_SUCCESS) hexdump("pubkey_xy", xy, sizeof(xy)); else printf("GET_PUBKEY_XY failed: 0x%x\n", r);
  // SIGN
  memset(&op,0,sizeof(op)); uint8_t msg[32]; memset(msg,0xab,sizeof msg); uint8_t sig[128];
  op.paramTypes=TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,TEEC_MEMREF_TEMP_OUTPUT,TEEC_NONE,TEEC_NONE);
  op.params[0].tmpref.buffer=msg; op.params[0].tmpref.size=sizeof(msg);
  op.params[1].tmpref.buffer=sig; op.params[1].tmpref.size=sizeof(sig);
  r = TEEC_InvokeCommand(&sess, FUS_CMD_SIGN, &op, NULL);
  if(r==TEEC_SUCCESS) hexdump("sig_der", sig, op.params[1].tmpref.size); else printf("KEY_SIGN failed: 0x%x\n", r);

  TEEC_CloseSession(&sess); TEEC_FinalizeContext(&ctx);
  return 0;
}
