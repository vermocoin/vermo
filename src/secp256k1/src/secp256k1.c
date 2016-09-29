/**********************************************************************
 * Copyright (c) 2013-2015 Pieter Wuille                              *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#define SECP256K1_BUILD (1)

#include <string.h>

#include "../include/secp256k1.h"

#include "util.h"
#include "num_impl.h"
#include "field_impl.h"
#include "scalar_impl.h"
#include "group_impl.h"
#include "ecmult_impl.h"
#include "ecmult_gen_impl.h"
#include "ecdsa_impl.h"
#include "schnorr_impl.h"
#include "eckey_impl.h"
#include "hash_impl.h"

struct secp256k1_context_struct {
    secp256k1_ecmult_context_t ecmult_ctx;
    secp256k1_ecmult_gen_context_t ecmult_gen_ctx;
};

secp256k1_context_t* secp256k1_context_create(int flags) {
    secp256k1_context_t* ret = (secp256k1_context_t*)checked_malloc(sizeof(secp256k1_context_t));

    secp256k1_ecmult_context_init(&ret->ecmult_ctx);
    secp256k1_ecmult_gen_context_init(&ret->ecmult_gen_ctx);

    if (flags & SECP256K1_CONTEXT_SIGN) {
        secp256k1_ecmult_gen_context_build(&ret->ecmult_gen_ctx);
    }
    if (flags & SECP256K1_CONTEXT_VERIFY) {
        secp256k1_ecmult_context_build(&ret->ecmult_ctx);
    }

    return ret;
}

secp256k1_context_t* secp256k1_context_clone(const secp256k1_context_t* ctx) {
    secp256k1_context_t* ret = (secp256k1_context_t*)checked_malloc(sizeof(secp256k1_context_t));
    secp256k1_ecmult_context_clone(&ret->ecmult_ctx, &ctx->ecmult_ctx);
    secp256k1_ecmult_gen_context_clone(&ret->ecmult_gen_ctx, &ctx->ecmult_gen_ctx);
    return ret;
}

void secp256k1_context_destroy(secp256k1_context_t* ctx) {
    secp256k1_ecmult_context_clear(&ctx->ecmult_ctx);
    secp256k1_ecmult_gen_context_clear(&ctx->ecmult_gen_ctx);

    free(ctx);
}

int secp256k1_ecdsa_verify(const secp256k1_context_t* ctx, const unsigned char *msg32, const unsigned char *sig, int siglen, const unsigned char *pubkey, int pubkeylen) {
    secp256k1_ge_t q;
    secp256k1_ecdsa_sig_t s;
    secp256k1_scalar_t m;
    int ret = -3;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig != NULL);
    DEBUG_CHECK(pubkey != NULL);

    secp256k1_scalar_set_b32(&m, msg32, NULL);

    if (secp256k1_eckey_pubkey_parse(&q, pubkey, pubkeylen)) {
        if (secp256k1_ecdsa_sig_parse(&s, sig, siglen)) {
            if (secp256k1_ecdsa_sig_verify(&ctx->ecmult_ctx, &s, &q, &m)) {
                /* success is 1, all other values are fail */
                ret = 1;
            } else {
                ret = 0;
            }
        } else {
            ret = -2;
        }
    } else {
        ret = -1;
    }

    return ret;
}

static int nonce_function_rfc6979(unsigned char *nonce32, const unsigned char *msg32, const unsigned char *key32, const char *algo, unsigned int counter, const void *data) {
   unsigned char keydata[128];
   secp256k1_rfc6979_hmac_sha256_t rng;
   unsigned int i;
   unsigned int pos = 0;
   memcpy(keydata, key32, 32);
   memcpy(keydata + 32, msg32, 32);
   pos = 64;
   if (data != NULL) {
       memcpy(keydata + pos, data, 32);
       pos += 32;
   }
   if (algo) {
       unsigned int len = strlen(algo);
       memcpy(keydata + pos, algo, len);
       pos += len;
   }
   secp256k1_rfc6979_hmac_sha256_initialize(&rng, keydata, pos);
   memset(keydata, 0, sizeof(keydata));
   for (i = 0; i <= counter; i++) {
       secp256k1_rfc6979_hmac_sha256_generate(&rng, nonce32, 32);
   }
   secp256k1_rfc6979_hmac_sha256_finalize(&rng);
   return 1;
}

const secp256k1_nonce_function_t secp256k1_nonce_function_rfc6979 = nonce_function_rfc6979;
const secp256k1_nonce_function_t secp256k1_nonce_function_default = nonce_function_rfc6979;

int secp256k1_ecdsa_sign(const secp256k1_context_t* ctx, const unsigned char *msg32, unsigned char *signature, int *signaturelen, const unsigned char *seckey, secp256k1_nonce_function_t noncefp, const void* noncedata) {
    secp256k1_ecdsa_sig_t sig;
    secp256k1_scalar_t sec, non, msg;
    int ret = 0;
    int overflow = 0;
    unsigned int count = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(signature != NULL);
    DEBUG_CHECK(signaturelen != NULL);
    DEBUG_CHECK(seckey != NULL);
    if (noncefp == NULL) {
        noncefp = secp256k1_nonce_function_default;
    }

    secp256k1_scalar_set_b32(&sec, seckey, &overflow);
    /* Fail if the secret key is invalid. */
    if (!overflow && !secp256k1_scalar_is_zero(&sec)) {
        secp256k1_scalar_set_b32(&msg, msg32, NULL);
        while (1) {
            unsigned char nonce32[32];
            ret = noncefp(nonce32, msg32, seckey, "", count, noncedata);
            if (!ret) {
                break;
            }
            secp256k1_scalar_set_b32(&non, nonce32, &overflow);
            memset(nonce32, 0, 32);
            if (!secp256k1_scalar_is_zero(&non) && !overflow) {
                if (secp256k1_ecdsa_sig_sign(&ctx->ecmult_gen_ctx, &sig, &sec, &msg, &non, NULL)) {
                    break;
                }
            }
            count++;
        }
        if (ret) {
            ret = secp256k1_ecdsa_sig_serialize(signature, signaturelen, &sig);
        }
        secp256k1_scalar_clear(&msg);
        secp256k1_scalar_clear(&non);
        secp256k1_scalar_clear(&sec);
    }
    if (!ret) {
        *signaturelen = 0;
    }
    return ret;
}

int secp256k1_ecdsa_sign_compact(const secp256k1_context_t* ctx, const unsigned char *msg32, unsigned char *sig64, const unsigned char *seckey, secp256k1_nonce_function_t noncefp, const void* noncedata, int *recid) {
    secp256k1_ecdsa_sig_t sig;
    secp256k1_scalar_t sec, non, msg;
    int ret = 0;
    int overflow = 0;
    unsigned int count = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(seckey != NULL);
    if (noncefp == NULL) {
        noncefp = secp256k1_nonce_function_default;
    }

    secp256k1_scalar_set_b32(&sec, seckey, &overflow);
    /* Fail if the secret key is invalid. */
    if (!overflow && !secp256k1_scalar_is_zero(&sec)) {
        secp256k1_scalar_set_b32(&msg, msg32, NULL);
        while (1) {
            unsigned char nonce32[32];
            ret = noncefp(nonce32, msg32, seckey, "", count, noncedata);
            if (!ret) {
                break;
            }
            secp256k1_scalar_set_b32(&non, nonce32, &overflow);
            memset(nonce32, 0, 32);
            if (!secp256k1_scalar_is_zero(&non) && !overflow) {
                if (secp256k1_ecdsa_sig_sign(&ctx->ecmult_gen_ctx, &sig, &sec, &msg, &non, recid)) {
                    break;
                }
            }
            count++;
        }
        if (ret) {
            secp256k1_scalar_get_b32(sig64, &sig.r);
            secp256k1_scalar_get_b32(sig64 + 32, &sig.s);
        }
        secp256k1_scalar_clear(&msg);
        secp256k1_scalar_clear(&non);
        secp256k1_scalar_clear(&sec);
    }
    if (!ret) {
        memset(sig64, 0, 64);
    }
    return ret;
}

int secp256k1_ecdsa_recover_compact(const secp256k1_context_t* ctx, const unsigned char *msg32, const unsigned char *sig64, unsigned char *pubkey, int *pubkeylen, int compressed, int recid) {
    secp256k1_ge_t q;
    secp256k1_ecdsa_sig_t sig;
    secp256k1_scalar_t m;
    int ret = 0;
    int overflow = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(pubkey != NULL);
    DEBUG_CHECK(pubkeylen != NULL);
    DEBUG_CHECK(recid >= 0 && recid <= 3);

    secp256k1_scalar_set_b32(&sig.r, sig64, &overflow);
    if (!overflow) {
        secp256k1_scalar_set_b32(&sig.s, sig64 + 32, &overflow);
        if (!overflow) {
            secp256k1_scalar_set_b32(&m, msg32, NULL);

            if (secp256k1_ecdsa_sig_recover(&ctx->ecmult_ctx, &sig, &q, &m, recid)) {
                ret = secp256k1_eckey_pubkey_serialize(&q, pubkey, pubkeylen, compressed);
            }
        }
    }
    return ret;
}

int secp256k1_ec_seckey_verify(const secp256k1_context_t* ctx, const unsigned char *seckey) {
    secp256k1_scalar_t sec;
    int ret;
    int overflow;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(seckey != NULL);
    (void)ctx;

    secp256k1_scalar_set_b32(&sec, seckey, &overflow);
    ret = !secp256k1_scalar_is_zero(&sec) && !overflow;
    secp256k1_scalar_clear(&sec);
    return ret;
}

int secp256k1_ec_pubkey_verify(const secp256k1_context_t* ctx, const unsigned char *pubkey, int pubkeylen) {
    secp256k1_ge_t q;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(pubkey != NULL);
    (void)ctx;

    return secp256k1_eckey_pubkey_parse(&q, pubkey, pubkeylen);
}

int secp256k1_ec_pubkey_create(const secp256k1_context_t* ctx, unsigned char *pubkey, int *pubkeylen, const unsigned char *seckey, int compressed) {
    secp256k1_gej_t pj;
    secp256k1_ge_t p;
    secp256k1_scalar_t sec;
    int overflow;
    int ret = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(pubkey != NULL);
    DEBUG_CHECK(pubkeylen != NULL);
    DEBUG_CHECK(seckey != NULL);

    secp256k1_scalar_set_b32(&sec, seckey, &overflow);
    if (!overflow) {
        secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &pj, &sec);
        secp256k1_scalar_clear(&sec);
        secp256k1_ge_set_gej(&p, &pj);
        ret = secp256k1_eckey_pubkey_serialize(&p, pubkey, pubkeylen, compressed);
    }
    if (!ret) {
        *pubkeylen = 0;
    }
    return ret;
}

/*int secp256k1_ec_pubkey_decompress(const secp256k1_context_t* ctx, const unsigned char *pubkeyin, unsigned char *pubkeyout, int *pubkeylen) {
    secp256k1_ge_t p;
    int ret = 0;
    DEBUG_CHECK(pubkeyin != NULL);
    DEBUG_CHECK(pubkeyout != NULL);
    DEBUG_CHECK(pubkeylen != NULL);
    (void)ctx;

    if (secp256k1_eckey_pubkey_parse(&p, pubkeyin, *pubkeylen)) {
        ret = secp256k1_eckey_pubkey_serialize(&p, pubkeyout, pubkeylen, 0);
    }
    return ret;
}*/

int secp256k1_ec_pubkey_decompress(const secp256k1_context_t* ctx, unsigned char *pubkey, int *pubkeylen) {
secp256k1_ge_t p;
int ret = 0;
DEBUG_CHECK(pubkey != NULL);
DEBUG_CHECK(pubkeylen != NULL);
(void)ctx;
if (secp256k1_eckey_pubkey_parse(&p, pubkey, *pubkeylen)) {
ret = secp256k1_eckey_pubkey_serialize(&p, pubkey, pubkeylen, 0);
}
return ret;
}

int secp256k1_ec_privkey_tweak_add(const secp256k1_context_t* ctx, unsigned char *seckey, const unsigned char *tweak) {
    secp256k1_scalar_t term;
    secp256k1_scalar_t sec;
    int ret = 0;
    int overflow = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(seckey != NULL);
    DEBUG_CHECK(tweak != NULL);
    (void)ctx;

    secp256k1_scalar_set_b32(&term, tweak, &overflow);
    secp256k1_scalar_set_b32(&sec, seckey, NULL);

    ret = secp256k1_eckey_privkey_tweak_add(&sec, &term) && !overflow;
    if (ret) {
        secp256k1_scalar_get_b32(seckey, &sec);
    }

    secp256k1_scalar_clear(&sec);
    secp256k1_scalar_clear(&term);
    return ret;
}

int secp256k1_ec_pubkey_tweak_add(const secp256k1_context_t* ctx, unsigned char *pubkey, int pubkeylen, const unsigned char *tweak) {
    secp256k1_ge_t p;
    secp256k1_scalar_t term;
    int ret = 0;
    int overflow = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(pubkey != NULL);
    DEBUG_CHECK(tweak != NULL);

    secp256k1_scalar_set_b32(&term, tweak, &overflow);
    if (!overflow) {
        ret = secp256k1_eckey_pubkey_parse(&p, pubkey, pubkeylen);
        if (ret) {
            ret = secp256k1_eckey_pubkey_tweak_add(&ctx->ecmult_ctx, &p, &term);
        }
        if (ret) {
            int oldlen = pubkeylen;
            ret = secp256k1_eckey_pubkey_serialize(&p, pubkey, &pubkeylen, oldlen <= 33);
            VERIFY_CHECK(pubkeylen == oldlen);
        }
    }

    return ret;
}

int secp256k1_ec_privkey_tweak_mul(const secp256k1_context_t* ctx, unsigned char *seckey, const unsigned char *tweak) {
    secp256k1_scalar_t factor;
    secp256k1_scalar_t sec;
    int ret = 0;
    int overflow = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(seckey != NULL);
    DEBUG_CHECK(tweak != NULL);
    (void)ctx;

    secp256k1_scalar_set_b32(&factor, tweak, &overflow);
    secp256k1_scalar_set_b32(&sec, seckey, NULL);
    ret = secp256k1_eckey_privkey_tweak_mul(&sec, &factor) && !overflow;
    if (ret) {
        secp256k1_scalar_get_b32(seckey, &sec);
    }

    secp256k1_scalar_clear(&sec);
    secp256k1_scalar_clear(&factor);
    return ret;
}

int secp256k1_ec_pubkey_tweak_mul(const secp256k1_context_t* ctx, unsigned char *pubkey, int pubkeylen, const unsigned char *tweak) {
    secp256k1_ge_t p;
    secp256k1_scalar_t factor;
    int ret = 0;
    int overflow = 0;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(pubkey != NULL);
    DEBUG_CHECK(tweak != NULL);

    secp256k1_scalar_set_b32(&factor, tweak, &overflow);
    if (!overflow) {
        ret = secp256k1_eckey_pubkey_parse(&p, pubkey, pubkeylen);
        if (ret) {
            ret = secp256k1_eckey_pubkey_tweak_mul(&ctx->ecmult_ctx, &p, &factor);
        }
        if (ret) {
            int oldlen = pubkeylen;
            ret = secp256k1_eckey_pubkey_serialize(&p, pubkey, &pubkeylen, oldlen <= 33);
            VERIFY_CHECK(pubkeylen == oldlen);
        }
    }

    return ret;
}

int secp256k1_ec_privkey_export(const secp256k1_context_t* ctx, const unsigned char *seckey, unsigned char *privkey, int *privkeylen, int compressed) {
    secp256k1_scalar_t key;
    int ret = 0;
    DEBUG_CHECK(seckey != NULL);
    DEBUG_CHECK(privkey != NULL);
    DEBUG_CHECK(privkeylen != NULL);
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));

    secp256k1_scalar_set_b32(&key, seckey, NULL);
    ret = secp256k1_eckey_privkey_serialize(&ctx->ecmult_gen_ctx, privkey, privkeylen, &key, compressed);
    secp256k1_scalar_clear(&key);
    return ret;
}

int secp256k1_ec_privkey_import(const secp256k1_context_t* ctx, unsigned char *seckey, const unsigned char *privkey, int privkeylen) {
    secp256k1_scalar_t key;
    int ret = 0;
    DEBUG_CHECK(seckey != NULL);
    DEBUG_CHECK(privkey != NULL);
    (void)ctx;

    ret = secp256k1_eckey_privkey_parse(&key, privkey, privkeylen);
    if (ret) {
        secp256k1_scalar_get_b32(seckey, &key);
    }
    secp256k1_scalar_clear(&key);
    return ret;
}

int secp256k1_context_randomize(secp256k1_context_t* ctx, const unsigned char *seed32) {
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    secp256k1_ecmult_gen_blind(&ctx->ecmult_gen_ctx, seed32);
    return 1;
}

void secp256k1_schnorr_msghash_sha256(unsigned char *h32, const unsigned char *r32, const unsigned char *msg32) {
    secp256k1_sha256_t sha;
    secp256k1_sha256_initialize(&sha);
    secp256k1_sha256_write(&sha, r32, 32);
    secp256k1_sha256_write(&sha, msg32, 32);
    secp256k1_sha256_finalize(&sha, h32);
}

static char *secp256k1_schnorr_message_tweak = "secp256k1-Schnorr-SHA256";

int secp256k1_schnorr_sign(const secp256k1_context_t* ctx, const unsigned char *msg32, unsigned char *sig64, const unsigned char *seckey, secp256k1_nonce_function_t noncefp, const void* noncedata) {
    secp256k1_scalar_t sec, non;
    int ret = 0;
    int overflow = 0;
    unsigned int count = 0;
    unsigned char msgt32[32];
    int i;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(seckey != NULL);
    if (noncefp == NULL) {
        noncefp = secp256k1_nonce_function_default;
    }

    for (i = 0; i < 32; i++) {
        /* Tweak the message to prevent a key leak from signing the same message
           using different algorithms. */
        msgt32[i] = msg32[i] ^ secp256k1_schnorr_message_tweak[i];
    }

    secp256k1_scalar_set_b32(&sec, seckey, NULL);
    while (1) {
        unsigned char nonce32[32];
        ret = noncefp(nonce32, msgt32, seckey, secp256k1_schnorr_message_tweak, count, noncedata);
        if (!ret) {
            break;
        }
        secp256k1_scalar_set_b32(&non, nonce32, &overflow);
        memset(nonce32, 0, 32);
        if (!secp256k1_scalar_is_zero(&non) && !overflow) {
            if (secp256k1_schnorr_sig_sign(&ctx->ecmult_gen_ctx, sig64, &sec, &non, NULL, secp256k1_schnorr_msghash_sha256, msg32)) {
                break;
            }
        }
        count++;
    }
    memset(msgt32, 0, 32);
    secp256k1_scalar_clear(&non);
    secp256k1_scalar_clear(&sec);
    return ret;
}

int secp256k1_schnorr_verify(const secp256k1_context_t* ctx, const unsigned char *msg32, const unsigned char *sig64, const unsigned char *pubkey, int pubkeylen) {
    secp256k1_ge_t q;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(pubkey != NULL);

    if (!secp256k1_eckey_pubkey_parse(&q, pubkey, pubkeylen)) {
        return -1;
    }
    return secp256k1_schnorr_sig_verify(&ctx->ecmult_ctx, sig64, &q, secp256k1_schnorr_msghash_sha256, msg32);
}

int secp256k1_schnorr_recover(const secp256k1_context_t* ctx, const unsigned char *msg32, const unsigned char *sig64, unsigned char *pubkey, int *pubkeylen, int compressed) {
    secp256k1_ge_t q;
    int ret;

    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_context_is_built(&ctx->ecmult_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(pubkey != NULL);

    if (!secp256k1_schnorr_sig_recover(&ctx->ecmult_ctx, sig64, &q, secp256k1_schnorr_msghash_sha256, msg32)) {
        return 0;
    }
    ret = secp256k1_eckey_pubkey_serialize(&q, pubkey, pubkeylen, compressed);
    VERIFY_CHECK(ret == 1);
    return ret;
}

int secp256k1_schnorr_generate_nonce_pair(const secp256k1_context_t* ctx, const unsigned char *msg32, const unsigned char *sec32, secp256k1_nonce_function_t noncefp, const void* noncedata, unsigned char *pubnonce33, unsigned char *privnonce32) {
    int count = 0;
    int ret = 1;
    unsigned char msgt32[32];
    secp256k1_gej_t Qj;
    secp256k1_ge_t Q;
    secp256k1_scalar_t sec;

    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sec32 != NULL);
    DEBUG_CHECK(pubnonce33 != NULL);
    DEBUG_CHECK(privnonce32 != NULL);

    if (noncefp == NULL) {
        noncefp = secp256k1_nonce_function_default;
    }

    do {
        int overflow;
        int pubkeylen = 33;
        ret = noncefp(privnonce32, msgt32, sec32, secp256k1_schnorr_message_tweak, count++, noncedata);
        if (!ret) {
            break;
        }
        secp256k1_scalar_set_b32(&sec, privnonce32, &overflow);
        if (overflow || secp256k1_scalar_is_zero(&sec)) {
            continue;
        }
        secp256k1_ecmult_gen(&ctx->ecmult_gen_ctx, &Qj, &sec);
        secp256k1_ge_set_gej(&Q, &Qj);

        ret = secp256k1_eckey_pubkey_serialize(&Q, pubnonce33, &pubkeylen, 1);
        VERIFY_CHECK(ret); /* This cannot fail, as sec cannot be 0, so Q cannot be infinity. */
        VERIFY_CHECK(pubkeylen == 33);
        break;
    } while(1);

    memset(msgt32, 0, 32);
    secp256k1_scalar_clear(&sec);
    return ret;
}

int secp256k1_ec_pubkey_combine(const secp256k1_context_t* ctx, unsigned char *pubnonce33, int n, const unsigned char **pubnonce33s) {
    int i;
    secp256k1_gej_t Qj;
    secp256k1_ge_t Q;
    int pubkeylen = 33;

    (void)ctx;
    DEBUG_CHECK(pubnonce33 != NULL);
    DEBUG_CHECK(n >= 1);
    DEBUG_CHECK(pubnonce33s != NULL);

    secp256k1_gej_set_infinity(&Qj);

    for (i = 0; i < n; i++) {
        if (!secp256k1_eckey_pubkey_parse(&Q, pubnonce33s[i], 33)) {
            return -1;
        }
        secp256k1_gej_add_ge(&Qj, &Qj, &Q);
    }
    secp256k1_ge_set_gej(&Q, &Qj);
    return secp256k1_eckey_pubkey_serialize(&Q, pubnonce33, &pubkeylen, 1);
}

int secp256k1_schnorr_partial_sign(const secp256k1_context_t* ctx, const unsigned char *msg32, unsigned char *sig64, const unsigned char *sec32, const unsigned char *secnonce32, const unsigned char *pubnonce33) {
    int overflow = 0;
    secp256k1_scalar_t sec, non;
    secp256k1_ge_t pubnon;
    DEBUG_CHECK(ctx != NULL);
    DEBUG_CHECK(secp256k1_ecmult_gen_context_is_built(&ctx->ecmult_gen_ctx));
    DEBUG_CHECK(msg32 != NULL);
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(sec32 != NULL);
    DEBUG_CHECK(secnonce32 != NULL);
    DEBUG_CHECK(pubnonce33 != NULL);

    secp256k1_scalar_set_b32(&sec, sec32, &overflow);
    if (overflow || secp256k1_scalar_is_zero(&sec)) {
        return -1;
    }
    secp256k1_scalar_set_b32(&non, secnonce32, &overflow);
    if (overflow || secp256k1_scalar_is_zero(&non)) {
        return -1;
    }
    if (!secp256k1_eckey_pubkey_parse(&pubnon, pubnonce33, 33)) {
        return -1;
    }
    return secp256k1_schnorr_sig_sign(&ctx->ecmult_gen_ctx, sig64, &sec, &non, &pubnon, secp256k1_schnorr_msghash_sha256, msg32);
}

int secp256k1_schnorr_partial_combine(const secp256k1_context_t* ctx, unsigned char *sig64, int n, const unsigned char **sig64sin) {
    (void)ctx;
    DEBUG_CHECK(sig64 != NULL);
    DEBUG_CHECK(n >= 1);
    DEBUG_CHECK(sig64sin != NULL);
    return secp256k1_schnorr_sig_combine(sig64, n, sig64sin);
}
