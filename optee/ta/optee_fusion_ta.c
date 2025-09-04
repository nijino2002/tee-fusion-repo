#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "optee_fusion_ta.h"

/* Ephemeral P-256 key inside TA (PoC) */
static TEE_ObjectHandle g_key = TEE_HANDLE_NULL;

static TEE_Result ensure_key(void){
    if (g_key != TEE_HANDLE_NULL) return TEE_SUCCESS;
    TEE_Result res = TEE_AllocateTransientObject(TEE_TYPE_ECDSA_KEYPAIR, 256, &g_key);
    if (res != TEE_SUCCESS) return res;
    TEE_Attribute attrs[1];
    TEE_InitValueAttribute(&attrs[0], TEE_ATTR_ECC_CURVE, TEE_ECC_CURVE_NIST_P256, 0);
    res = TEE_GenerateKey(g_key, 256, attrs, 1);
    return res;
}

static TEE_Result cmd_key_gen(uint32_t ptypes, TEE_Param params[4]){
    (void)ptypes; (void)params;
    if (g_key != TEE_HANDLE_NULL) { TEE_FreeTransientObject(g_key); g_key = TEE_HANDLE_NULL; }
    return ensure_key();
}

static TEE_Result cmd_get_pubkey_xy(uint32_t ptypes, TEE_Param params[4]){
    if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
        return TEE_ERROR_BAD_PARAMETERS;
    TEE_Result res = ensure_key(); if (res != TEE_SUCCESS) return res;

    uint8_t x[32]={0}, y[32]={0}; uint32_t xl=sizeof(x), yl=sizeof(y);
    res = TEE_GetObjectBufferAttribute(g_key, TEE_ATTR_ECC_PUBLIC_VALUE_X, x, &xl); if (res != TEE_SUCCESS) return res;
    res = TEE_GetObjectBufferAttribute(g_key, TEE_ATTR_ECC_PUBLIC_VALUE_Y, y, &yl); if (res != TEE_SUCCESS) return res;

    if (params[0].memref.size < xl+yl) { params[0].memref.size = xl+yl; return TEE_ERROR_SHORT_BUFFER; }
    TEE_MemMove(params[0].memref.buffer, x, xl);
    TEE_MemMove((uint8_t*)params[0].memref.buffer + xl, y, yl);
    params[0].memref.size = xl + yl;
    return TEE_SUCCESS;
}

static TEE_Result cmd_key_sign(uint32_t ptypes, TEE_Param params[4]){
    if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
        return TEE_ERROR_BAD_PARAMETERS;
    TEE_Result res = ensure_key(); if (res != TEE_SUCCESS) return res;

    TEE_OperationHandle dig = TEE_HANDLE_NULL;
    TEE_OperationHandle sig = TEE_HANDLE_NULL;
    uint8_t digest[32]; uint32_t dlen = sizeof(digest);

    res = TEE_AllocateOperation(&dig, TEE_ALG_SHA256, TEE_MODE_DIGEST, 0);
    if (res != TEE_SUCCESS) goto out;
    TEE_DigestDoFinal(dig, params[0].memref.buffer, params[0].memref.size, digest, &dlen);

    /* OP-TEE uses hash-specific ECDSA algo for sign-digest */
    res = TEE_AllocateOperation(&sig, TEE_ALG_ECDSA_SHA256, TEE_MODE_SIGN, 256);
    if (res != TEE_SUCCESS) goto out;
    res = TEE_SetOperationKey(sig, g_key);
    if (res != TEE_SUCCESS) goto out;

    uint32_t out_sz = params[1].memref.size;
    res = TEE_AsymmetricSignDigest(sig, NULL, 0, digest, dlen, params[1].memref.buffer, &out_sz);
    if (res == TEE_ERROR_SHORT_BUFFER) { params[1].memref.size = out_sz; goto out; }
    if (res != TEE_SUCCESS) goto out;
    params[1].memref.size = out_sz;

out:
    if (dig) TEE_FreeOperation(dig);
    if (sig) TEE_FreeOperation(sig);
    return res;
}

static TEE_Result cmd_rand(uint32_t ptypes, TEE_Param params[4]){
    if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
        return TEE_ERROR_BAD_PARAMETERS;
    uint32_t need = params[0].value.a;
    if (params[1].memref.size < need) { params[1].memref.size = need; return TEE_ERROR_SHORT_BUFFER; }
    TEE_GenerateRandom(params[1].memref.buffer, need);
    params[1].memref.size = need;
    return TEE_SUCCESS;
}

static TEE_Result cmd_get_psa_token(uint32_t ptypes, TEE_Param params[4]){
    if (ptypes != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE))
        return TEE_ERROR_BAD_PARAMETERS;
    const char token[] = "OPTEE_PSA_TOKEN_POC";
    uint32_t need = sizeof(token)-1;
    if (params[0].memref.size < need) { params[0].memref.size = need; return TEE_ERROR_SHORT_BUFFER; }
    TEE_MemMove(params[0].memref.buffer, token, need);
    params[0].memref.size = need;
    return TEE_SUCCESS;
}

/* TA entry points */
TEE_Result TA_CreateEntryPoint(void){ return TEE_SUCCESS; }
void TA_DestroyEntryPoint(void){ if (g_key != TEE_HANDLE_NULL) { TEE_FreeTransientObject(g_key); g_key = TEE_HANDLE_NULL; } }
TEE_Result TA_OpenSessionEntryPoint(uint32_t ptypes, TEE_Param params[4], void** sess){ (void)ptypes; (void)params; (void)sess; return TEE_SUCCESS; }
void TA_CloseSessionEntryPoint(void* sess){ (void)sess; }
TEE_Result TA_InvokeCommandEntryPoint(void* sess, uint32_t cmd, uint32_t ptypes, TEE_Param params[4]){
    (void)sess;
    switch (cmd){
    case TA_CMD_GET_PSA_TOKEN:  return cmd_get_psa_token(ptypes, params);
    case TA_CMD_KEY_GEN:        return cmd_key_gen(ptypes, params);
    case TA_CMD_GET_PUBKEY_XY:  return cmd_get_pubkey_xy(ptypes, params);
    case TA_CMD_KEY_SIGN:       return cmd_key_sign(ptypes, params);
    case TA_CMD_RAND:           return cmd_rand(ptypes, params);
    default: return TEE_ERROR_NOT_SUPPORTED;
    }
}
