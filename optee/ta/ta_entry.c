#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "../../include/fusion_abi.h"
#include "../../secure_app/fusion_secure.h"

/* Thin TA entry that maps ABI commands to platform-agnostic secure_app */

TEE_Result TA_CreateEntryPoint(void){ fs_init(); return TEE_SUCCESS; }
void TA_DestroyEntryPoint(void){ }
TEE_Result TA_OpenSessionEntryPoint(uint32_t ptypes, TEE_Param params[4], void** sess){ (void)ptypes;(void)params;(void)sess; return TEE_SUCCESS; }
void TA_CloseSessionEntryPoint(void* sess){ (void)sess; }

static TEE_Result do_key_gen(uint32_t ptypes, TEE_Param params[4]){
  (void)ptypes; (void)params; return fs_key_gen()==0?TEE_SUCCESS:TEE_ERROR_GENERIC;
}
static TEE_Result do_get_pub_xy(uint32_t ptypes, TEE_Param params[4]){
  if(ptypes!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
    return TEE_ERROR_BAD_PARAMETERS;
  size_t out_len=params[0].memref.size; uint8_t* out=params[0].memref.buffer; int rc=fs_get_pubkey_xy(out,&out_len);
  if(rc==-2){ params[0].memref.size=out_len; return TEE_ERROR_SHORT_BUFFER; }
  if(rc!=0) return TEE_ERROR_GENERIC; params[0].memref.size=out_len; return TEE_SUCCESS;
}
static TEE_Result do_sign(uint32_t ptypes, TEE_Param params[4]){
  if(ptypes!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
    return TEE_ERROR_BAD_PARAMETERS;
  size_t sl=params[1].memref.size; int rc=fs_sign(params[0].memref.buffer, params[0].memref.size, params[1].memref.buffer, &sl);
  if(rc==-2){ params[1].memref.size=sl; return TEE_ERROR_SHORT_BUFFER; }
  if(rc!=0) return TEE_ERROR_GENERIC; params[1].memref.size=sl; return TEE_SUCCESS;
}
static TEE_Result do_verify(uint32_t ptypes, TEE_Param params[4]){
  if(ptypes!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE))
    return TEE_ERROR_BAD_PARAMETERS;
  int ok=fs_verify(params[0].memref.buffer, params[0].memref.size, params[1].memref.buffer, params[1].memref.size)==0;
  params[2].value.a = ok?1:0; return ok?TEE_SUCCESS:TEE_ERROR_SIGNATURE_INVALID;
}
static TEE_Result do_rand(uint32_t ptypes, TEE_Param params[4]){
  if(ptypes!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
    return TEE_ERROR_BAD_PARAMETERS;
  size_t need=params[0].value.a; if(params[1].memref.size<need){ params[1].memref.size=need; return TEE_ERROR_SHORT_BUFFER; }
  return sa_rand(params[1].memref.buffer, need)==0?TEE_SUCCESS:TEE_ERROR_GENERIC;
}
static TEE_Result do_token(uint32_t ptypes, TEE_Param params[4]){
  if(ptypes!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
    return TEE_ERROR_BAD_PARAMETERS;
  size_t n=params[0].memref.size; int rc=fs_get_token(params[0].memref.buffer,&n);
  if(rc==-2){ params[0].memref.size=n; return TEE_ERROR_SHORT_BUFFER; }
  if(rc!=0) return TEE_ERROR_GENERIC; params[0].memref.size=n; return TEE_SUCCESS;
}

TEE_Result TA_InvokeCommandEntryPoint(void* sess, uint32_t cmd, uint32_t ptypes, TEE_Param params[4]){
  (void)sess;
  switch(cmd){
    case FUS_CMD_GET_TOKEN:  return do_token(ptypes,params);
    case FUS_CMD_KEY_GEN:    return do_key_gen(ptypes,params);
    case FUS_CMD_GET_PUB_XY: return do_get_pub_xy(ptypes,params);
    case FUS_CMD_SIGN:       return do_sign(ptypes,params);
    case FUS_CMD_VERIFY:     return do_verify(ptypes,params);
    case FUS_CMD_RAND:       return do_rand(ptypes,params);
    default: return TEE_ERROR_NOT_SUPPORTED;
  }
}

