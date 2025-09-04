#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/objects.h>
#include <openssl/rand.h>
#include <tee_client_api.h>

#include "../adapter_iface.h"   /* expect this to exist in your repo */
#include "../../include/tee_fusion.h"
#include "optee_fusion_ta_uuid.h"
#include "optee_fusion_ta.h"

/* ---- OP-TEE CA session management ---- */
static TEEC_Context   g_ctx;
static TEEC_Session   g_sess;
static int            g_session_open = 0;

static int open_session(void){
    if (g_session_open) return 1;
    TEEC_Result res;
    TEEC_UUID uuid = TA_OPTEE_FUSION_UUID;
    res = TEEC_InitializeContext(NULL, &g_ctx);
    if (res != TEEC_SUCCESS) { fprintf(stderr, "[optee] TEEC_InitializeContext failed: 0x%x\n", res); return 0; }
    res = TEEC_OpenSession(&g_ctx, &g_sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
    if (res != TEEC_SUCCESS) { fprintf(stderr, "[optee] TEEC_OpenSession failed: 0x%x\n", res); TEEC_FinalizeContext(&g_ctx); return 0; }
    g_session_open = 1;
    return 1;
}
static void close_session(void){
    if (!g_session_open) return;
    TEEC_CloseSession(&g_sess);
    TEEC_FinalizeContext(&g_ctx);
    g_session_open = 0;
}

/* ---- Helper: convert raw X||Y to DER SPKI ---- */
static int pubkey_xy_to_der(const unsigned char* xy, size_t xy_len, unsigned char* out, size_t* out_len){
    int ret = 0;
    EC_KEY* eck = NULL;
    EVP_PKEY* pkey = NULL;
    const EC_GROUP* grp = NULL;
    BN_CTX* bnctx = NULL;
    BIGNUM *x=NULL, *y=NULL;

    if (!xy || xy_len != 64 || !out_len) return 0;
    eck = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!eck) goto done;
    grp = EC_KEY_get0_group(eck);
    bnctx = BN_CTX_new();
    x = BN_bin2bn(xy, 32, NULL);
    y = BN_bin2bn(xy+32, 32, NULL);
    if (!x || !y) goto done;

    EC_POINT* pt = EC_POINT_new(grp);
    if (!pt) goto done;
    if (!EC_POINT_set_affine_coordinates_GFp(grp, pt, x, y, bnctx)) { EC_POINT_free(pt); goto done; }
    if (!EC_KEY_set_public_key(eck, pt)) { EC_POINT_free(pt); goto done; }
    EC_POINT_free(pt);

    pkey = EVP_PKEY_new();
    if (!pkey) goto done;
    if (!EVP_PKEY_assign_EC_KEY(pkey, eck)) goto done; /* pkey now owns eck */
    eck = NULL;

    int len = i2d_PUBKEY(pkey, NULL);
    if (len <= 0 || (size_t)len > *out_len) goto done;
    unsigned char* p = out;
    len = i2d_PUBKEY(pkey, &p);
    *out_len = (size_t)len;
    ret = 1;

done:
    if (eck) EC_KEY_free(eck);
    if (pkey) EVP_PKEY_free(pkey);
    if (bnctx) BN_CTX_free(bnctx);
    if (x) BN_free(x);
    if (y) BN_free(y);
    return ret;
}

/* ---- Adapter vtable ---- */

static tee_status_t get_report(tee_buf_t* out){
    if (!out) return TEE_EINVAL;
    if (!open_session()) return TEE_EINTERNAL;
    TEEC_Operation op; memset(&op, 0, sizeof(op));
    uint8_t buf[4096];
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = buf;
    op.params[0].tmpref.size = sizeof(buf);
    TEEC_Result r = TEEC_InvokeCommand(&g_sess, TA_CMD_GET_PSA_TOKEN, &op, NULL);
    if (r != TEEC_SUCCESS) { fprintf(stderr, "[optee] GET_PSA_TOKEN failed: 0x%x\n", r); return TEE_EINTERNAL; }
    size_t n = op.params[0].tmpref.size;
    uint8_t* p = (uint8_t*)malloc(n);
    if (!p) return TEE_ENOMEM;
    memcpy(p, buf, n);
    out->ptr = p; out->len = n;
    return TEE_OK;
}

static tee_status_t fill_platform_claims(const uint8_t* token, size_t n){
    extern void mapping_reset();
    extern void mapping_set_common(const char*, const char*, int, unsigned int);
    extern void mapping_set_sw_measurement(const uint8_t*, size_t);
    extern void mapping_set_native_quote(const uint8_t*, size_t);
    mapping_reset();
    mapping_set_common("ARM-OPTEE", "secure-world", 0, 1);
    uint8_t meas[32] = {0}; /* TODO: parse measurement from PSA/FF-A token */
    mapping_set_sw_measurement(meas, sizeof(meas));
    if (token && n) mapping_set_native_quote(token, n);
    return TEE_OK;
}

static tee_status_t key_new(tee_key_algo_t algo, tee_attested_key_t* out){
    if (!out || algo != TEE_EC_P256) return TEE_EINVAL;
    if (!open_session()) return TEE_EINTERNAL;

    TEEC_Operation op; memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    TEEC_Result r = TEEC_InvokeCommand(&g_sess, TA_CMD_KEY_GEN, &op, NULL);
    if (r != TEEC_SUCCESS) { fprintf(stderr, "[optee] KEY_GEN failed: 0x%x\n", r); return TEE_EINTERNAL; }

    uint8_t xy[64]; memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = xy; op.params[0].tmpref.size = sizeof(xy);
    r = TEEC_InvokeCommand(&g_sess, TA_CMD_GET_PUBKEY_XY, &op, NULL);
    if (r != TEEC_SUCCESS) { fprintf(stderr, "[optee] GET_PUBKEY_XY failed: 0x%x\n", r); return TEE_EINTERNAL; }

    size_t der_len = sizeof(out->pubkey);
    if (!pubkey_xy_to_der(xy, sizeof(xy), out->pubkey, &der_len)) return TEE_EINTERNAL;
    out->algo = TEE_EC_P256; out->pubkey_len = der_len;
    return TEE_OK;
}

static tee_status_t key_sign(const void* m, size_t n, uint8_t* sig, size_t* slen){
    if (!m || !sig || !slen) return TEE_EINVAL;
    if (!open_session()) return TEE_EINTERNAL;
    TEEC_Operation op; memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = (void*)m; op.params[0].tmpref.size = n;
    op.params[1].tmpref.buffer = sig;      op.params[1].tmpref.size = *slen;
    TEEC_Result r = TEEC_InvokeCommand(&g_sess, TA_CMD_KEY_SIGN, &op, NULL);
    if (r == TEE_ERROR_SHORT_BUFFER) { *slen = op.params[1].tmpref.size; return TEE_ENOMEM; }
    if (r != TEEC_SUCCESS) { fprintf(stderr, "[optee] KEY_SIGN failed: 0x%x\n", r); return TEE_EINTERNAL; }
    *slen = op.params[1].tmpref.size;
    return TEE_OK;
}

static tee_status_t rand_bytes(void* buf, size_t len){
    if (!buf || len == 0) return TEE_EINVAL;
    if (!open_session()) return TEE_EINTERNAL;
    TEEC_Operation op; memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_MEMREF_TEMP_OUTPUT, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = (uint32_t)len;
    op.params[1].tmpref.buffer = buf; op.params[1].tmpref.size = len;
    TEEC_Result r = TEEC_InvokeCommand(&g_sess, TA_CMD_RAND, &op, NULL);
    if (r != TEEC_SUCCESS) { fprintf(stderr, "[optee] RAND failed: 0x%x\n", r); return TEE_EINTERNAL; }
    return TEE_OK;
}

static tee_status_t ocall(uint32_t op, const void* in, size_t ilen, void* out, size_t* olen){
    (void)op;
    if (!out || !olen) return TEE_EINVAL;
    size_t n = ilen < *olen ? ilen : *olen;
    if (n && in) memcpy(out, in, n);
    *olen = n;
    return TEE_OK;
}

static tee_class_t platform_id(void){ return TEE_CLASS_TRUSTZONE; }

void tee_register_active_adapter(tee_adapter_vt* vt){
    vt->get_report = get_report;
    vt->fill_platform_claims = fill_platform_claims;
    vt->key_new  = key_new;
    vt->key_sign = key_sign;
    vt->rand_bytes = rand_bytes;
    vt->ocall = ocall;
    vt->platform_id = platform_id;
    atexit(close_session);
}
