/***********************************************************************
 * Copyright (c) 2013, 2014 Pieter Wuille                              *
 * Distributed under the MIT software license, see the accompanying    *
 * file COPYING or https://www.opensource.org/licenses/mit-license.php.*
 ***********************************************************************/

#ifndef SECP256K1_SCALAR_REPR_IMPL_H
#define SECP256K1_SCALAR_REPR_IMPL_H

#include "checkmem.h"
#include "int128.h"
#include "modinv64_impl.h"
#include "util.h"

/* Limbs of the secp256k1 order. */
#define SECP256K1_N_0 ((uint64_t)0xBFD25E8CD0364141ULL)
#define SECP256K1_N_1 ((uint64_t)0xBAAEDCE6AF48A03BULL)
#define SECP256K1_N_2 ((uint64_t)0xFFFFFFFFFFFFFFFEULL)
#define SECP256K1_N_3 ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

/* Limbs of 2^256 minus the secp256k1 order. */
#define SECP256K1_N_C_0 (~SECP256K1_N_0 + 1)
#define SECP256K1_N_C_1 (~SECP256K1_N_1)
#define SECP256K1_N_C_2 (1)

/* Limbs of half the secp256k1 order. */
#define SECP256K1_N_H_0 ((uint64_t)0xDFE92F46681B20A0ULL)
#define SECP256K1_N_H_1 ((uint64_t)0x5D576E7357A4501DULL)
#define SECP256K1_N_H_2 ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#define SECP256K1_N_H_3 ((uint64_t)0x7FFFFFFFFFFFFFFFULL)

SECP256K1_INLINE static void rustsecp256k1_v0_9_1_scalar_clear(rustsecp256k1_v0_9_1_scalar *r) {
    r->d[0] = 0;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;
}

SECP256K1_INLINE static void rustsecp256k1_v0_9_1_scalar_set_int(rustsecp256k1_v0_9_1_scalar *r, unsigned int v) {
    r->d[0] = v;
    r->d[1] = 0;
    r->d[2] = 0;
    r->d[3] = 0;

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

SECP256K1_INLINE static unsigned int rustsecp256k1_v0_9_1_scalar_get_bits(const rustsecp256k1_v0_9_1_scalar *a, unsigned int offset, unsigned int count) {
    rustsecp256k1_v0_9_1_scalar_verify(a);
    VERIFY_CHECK((offset + count - 1) >> 6 == offset >> 6);

    return (a->d[offset >> 6] >> (offset & 0x3F)) & ((((uint64_t)1) << count) - 1);
}

SECP256K1_INLINE static unsigned int rustsecp256k1_v0_9_1_scalar_get_bits_var(const rustsecp256k1_v0_9_1_scalar *a, unsigned int offset, unsigned int count) {
    rustsecp256k1_v0_9_1_scalar_verify(a);
    VERIFY_CHECK(count < 32);
    VERIFY_CHECK(offset + count <= 256);

    if ((offset + count - 1) >> 6 == offset >> 6) {
        return rustsecp256k1_v0_9_1_scalar_get_bits(a, offset, count);
    } else {
        VERIFY_CHECK((offset >> 6) + 1 < 4);
        return ((a->d[offset >> 6] >> (offset & 0x3F)) | (a->d[(offset >> 6) + 1] << (64 - (offset & 0x3F)))) & ((((uint64_t)1) << count) - 1);
    }
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_check_overflow(const rustsecp256k1_v0_9_1_scalar *a) {
    int yes = 0;
    int no = 0;
    no |= (a->d[3] < SECP256K1_N_3); /* No need for a > check. */
    no |= (a->d[2] < SECP256K1_N_2);
    yes |= (a->d[2] > SECP256K1_N_2) & ~no;
    no |= (a->d[1] < SECP256K1_N_1);
    yes |= (a->d[1] > SECP256K1_N_1) & ~no;
    yes |= (a->d[0] >= SECP256K1_N_0) & ~no;
    return yes;
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_reduce(rustsecp256k1_v0_9_1_scalar *r, unsigned int overflow) {
    rustsecp256k1_v0_9_1_uint128 t;
    VERIFY_CHECK(overflow <= 1);

    rustsecp256k1_v0_9_1_u128_from_u64(&t, r->d[0]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, overflow * SECP256K1_N_C_0);
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[1]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, overflow * SECP256K1_N_C_1);
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[2]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, overflow * SECP256K1_N_C_2);
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[3]);
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&t);

    rustsecp256k1_v0_9_1_scalar_verify(r);
    return overflow;
}

static int rustsecp256k1_v0_9_1_scalar_add(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *a, const rustsecp256k1_v0_9_1_scalar *b) {
    int overflow;
    rustsecp256k1_v0_9_1_uint128 t;
    rustsecp256k1_v0_9_1_scalar_verify(a);
    rustsecp256k1_v0_9_1_scalar_verify(b);

    rustsecp256k1_v0_9_1_u128_from_u64(&t, a->d[0]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, b->d[0]);
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, a->d[1]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, b->d[1]);
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, a->d[2]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, b->d[2]);
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, a->d[3]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, b->d[3]);
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    overflow = rustsecp256k1_v0_9_1_u128_to_u64(&t) + rustsecp256k1_v0_9_1_scalar_check_overflow(r);
    VERIFY_CHECK(overflow == 0 || overflow == 1);
    rustsecp256k1_v0_9_1_scalar_reduce(r, overflow);

    rustsecp256k1_v0_9_1_scalar_verify(r);
    return overflow;
}

static void rustsecp256k1_v0_9_1_scalar_cadd_bit(rustsecp256k1_v0_9_1_scalar *r, unsigned int bit, int flag) {
    rustsecp256k1_v0_9_1_uint128 t;
    volatile int vflag = flag;
    rustsecp256k1_v0_9_1_scalar_verify(r);
    VERIFY_CHECK(bit < 256);

    bit += ((uint32_t) vflag - 1) & 0x100;  /* forcing (bit >> 6) > 3 makes this a noop */
    rustsecp256k1_v0_9_1_u128_from_u64(&t, r->d[0]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ((uint64_t)((bit >> 6) == 0)) << (bit & 0x3F));
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[1]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ((uint64_t)((bit >> 6) == 1)) << (bit & 0x3F));
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[2]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ((uint64_t)((bit >> 6) == 2)) << (bit & 0x3F));
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&t); rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[3]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ((uint64_t)((bit >> 6) == 3)) << (bit & 0x3F));
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&t);

    rustsecp256k1_v0_9_1_scalar_verify(r);
#ifdef VERIFY
    VERIFY_CHECK(rustsecp256k1_v0_9_1_u128_hi_u64(&t) == 0);
#endif
}

static void rustsecp256k1_v0_9_1_scalar_set_b32(rustsecp256k1_v0_9_1_scalar *r, const unsigned char *b32, int *overflow) {
    int over;
    r->d[0] = rustsecp256k1_v0_9_1_read_be64(&b32[24]);
    r->d[1] = rustsecp256k1_v0_9_1_read_be64(&b32[16]);
    r->d[2] = rustsecp256k1_v0_9_1_read_be64(&b32[8]);
    r->d[3] = rustsecp256k1_v0_9_1_read_be64(&b32[0]);
    over = rustsecp256k1_v0_9_1_scalar_reduce(r, rustsecp256k1_v0_9_1_scalar_check_overflow(r));
    if (overflow) {
        *overflow = over;
    }

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

static void rustsecp256k1_v0_9_1_scalar_get_b32(unsigned char *bin, const rustsecp256k1_v0_9_1_scalar* a) {
    rustsecp256k1_v0_9_1_scalar_verify(a);

    rustsecp256k1_v0_9_1_write_be64(&bin[0],  a->d[3]);
    rustsecp256k1_v0_9_1_write_be64(&bin[8],  a->d[2]);
    rustsecp256k1_v0_9_1_write_be64(&bin[16], a->d[1]);
    rustsecp256k1_v0_9_1_write_be64(&bin[24], a->d[0]);
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_is_zero(const rustsecp256k1_v0_9_1_scalar *a) {
    rustsecp256k1_v0_9_1_scalar_verify(a);

    return (a->d[0] | a->d[1] | a->d[2] | a->d[3]) == 0;
}

static void rustsecp256k1_v0_9_1_scalar_negate(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *a) {
    uint64_t nonzero = 0xFFFFFFFFFFFFFFFFULL * (rustsecp256k1_v0_9_1_scalar_is_zero(a) == 0);
    rustsecp256k1_v0_9_1_uint128 t;
    rustsecp256k1_v0_9_1_scalar_verify(a);

    rustsecp256k1_v0_9_1_u128_from_u64(&t, ~a->d[0]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_0 + 1);
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ~a->d[1]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_1);
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ~a->d[2]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_2);
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, ~a->d[3]);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_3);
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero;

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_is_one(const rustsecp256k1_v0_9_1_scalar *a) {
    rustsecp256k1_v0_9_1_scalar_verify(a);

    return ((a->d[0] ^ 1) | a->d[1] | a->d[2] | a->d[3]) == 0;
}

static int rustsecp256k1_v0_9_1_scalar_is_high(const rustsecp256k1_v0_9_1_scalar *a) {
    int yes = 0;
    int no = 0;
    rustsecp256k1_v0_9_1_scalar_verify(a);

    no |= (a->d[3] < SECP256K1_N_H_3);
    yes |= (a->d[3] > SECP256K1_N_H_3) & ~no;
    no |= (a->d[2] < SECP256K1_N_H_2) & ~yes; /* No need for a > check. */
    no |= (a->d[1] < SECP256K1_N_H_1) & ~yes;
    yes |= (a->d[1] > SECP256K1_N_H_1) & ~no;
    yes |= (a->d[0] > SECP256K1_N_H_0) & ~no;
    return yes;
}

static int rustsecp256k1_v0_9_1_scalar_cond_negate(rustsecp256k1_v0_9_1_scalar *r, int flag) {
    /* If we are flag = 0, mask = 00...00 and this is a no-op;
     * if we are flag = 1, mask = 11...11 and this is identical to rustsecp256k1_v0_9_1_scalar_negate */
    volatile int vflag = flag;
    uint64_t mask = -vflag;
    uint64_t nonzero = (rustsecp256k1_v0_9_1_scalar_is_zero(r) != 0) - 1;
    rustsecp256k1_v0_9_1_uint128 t;
    rustsecp256k1_v0_9_1_scalar_verify(r);

    rustsecp256k1_v0_9_1_u128_from_u64(&t, r->d[0] ^ mask);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, (SECP256K1_N_0 + 1) & mask);
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[1] ^ mask);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_1 & mask);
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[2] ^ mask);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_2 & mask);
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero; rustsecp256k1_v0_9_1_u128_rshift(&t, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, r->d[3] ^ mask);
    rustsecp256k1_v0_9_1_u128_accum_u64(&t, SECP256K1_N_3 & mask);
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&t) & nonzero;

    rustsecp256k1_v0_9_1_scalar_verify(r);
    return 2 * (mask == 0) - 1;
}

/* Inspired by the macros in OpenSSL's crypto/bn/asm/x86_64-gcc.c. */

/** Add a*b to the number defined by (c0,c1,c2). c2 must never overflow. */
#define muladd(a,b) { \
    uint64_t tl, th; \
    { \
        rustsecp256k1_v0_9_1_uint128 t; \
        rustsecp256k1_v0_9_1_u128_mul(&t, a, b); \
        th = rustsecp256k1_v0_9_1_u128_hi_u64(&t);  /* at most 0xFFFFFFFFFFFFFFFE */ \
        tl = rustsecp256k1_v0_9_1_u128_to_u64(&t); \
    } \
    c0 += tl;                 /* overflow is handled on the next line */ \
    th += (c0 < tl);          /* at most 0xFFFFFFFFFFFFFFFF */ \
    c1 += th;                 /* overflow is handled on the next line */ \
    c2 += (c1 < th);          /* never overflows by contract (verified in the next line) */ \
    VERIFY_CHECK((c1 >= th) || (c2 != 0)); \
}

/** Add a*b to the number defined by (c0,c1). c1 must never overflow. */
#define muladd_fast(a,b) { \
    uint64_t tl, th; \
    { \
        rustsecp256k1_v0_9_1_uint128 t; \
        rustsecp256k1_v0_9_1_u128_mul(&t, a, b); \
        th = rustsecp256k1_v0_9_1_u128_hi_u64(&t);  /* at most 0xFFFFFFFFFFFFFFFE */ \
        tl = rustsecp256k1_v0_9_1_u128_to_u64(&t); \
    } \
    c0 += tl;                 /* overflow is handled on the next line */ \
    th += (c0 < tl);          /* at most 0xFFFFFFFFFFFFFFFF */ \
    c1 += th;                 /* never overflows by contract (verified in the next line) */ \
    VERIFY_CHECK(c1 >= th); \
}

/** Add a to the number defined by (c0,c1,c2). c2 must never overflow. */
#define sumadd(a) { \
    unsigned int over; \
    c0 += (a);                  /* overflow is handled on the next line */ \
    over = (c0 < (a));         \
    c1 += over;                 /* overflow is handled on the next line */ \
    c2 += (c1 < over);          /* never overflows by contract */ \
}

/** Add a to the number defined by (c0,c1). c1 must never overflow, c2 must be zero. */
#define sumadd_fast(a) { \
    c0 += (a);                 /* overflow is handled on the next line */ \
    c1 += (c0 < (a));          /* never overflows by contract (verified the next line) */ \
    VERIFY_CHECK((c1 != 0) | (c0 >= (a))); \
    VERIFY_CHECK(c2 == 0); \
}

/** Extract the lowest 64 bits of (c0,c1,c2) into n, and left shift the number 64 bits. */
#define extract(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = c2; \
    c2 = 0; \
}

/** Extract the lowest 64 bits of (c0,c1,c2) into n, and left shift the number 64 bits. c2 is required to be zero. */
#define extract_fast(n) { \
    (n) = c0; \
    c0 = c1; \
    c1 = 0; \
    VERIFY_CHECK(c2 == 0); \
}

static void rustsecp256k1_v0_9_1_scalar_reduce_512(rustsecp256k1_v0_9_1_scalar *r, const uint64_t *l) {
#ifdef USE_ASM_X86_64
    /* Reduce 512 bits into 385. */
    uint64_t m0, m1, m2, m3, m4, m5, m6;
    uint64_t p0, p1, p2, p3, p4;
    uint64_t c;

    __asm__ __volatile__(
    /* Preload. */
    "movq 32(%%rsi), %%r11\n"
    "movq 40(%%rsi), %%r12\n"
    "movq 48(%%rsi), %%r13\n"
    "movq 56(%%rsi), %%r14\n"
    /* Initialize r8,r9,r10 */
    "movq 0(%%rsi), %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /* (r8,r9) += n0 * c0 */
    "movq %8, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /* extract m0 */
    "movq %%r8, %q0\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r10) += l1 */
    "addq 8(%%rsi), %%r9\n"
    "adcq $0, %%r10\n"
    /* (r9,r10,r8) += n1 * c0 */
    "movq %8, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += n0 * c1 */
    "movq %9, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* extract m1 */
    "movq %%r9, %q1\n"
    "xorq %%r9, %%r9\n"
    /* (r10,r8,r9) += l2 */
    "addq 16(%%rsi), %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += n2 * c0 */
    "movq %8, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += n1 * c1 */
    "movq %9, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += n0 */
    "addq %%r11, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /* extract m2 */
    "movq %%r10, %q2\n"
    "xorq %%r10, %%r10\n"
    /* (r8,r9,r10) += l3 */
    "addq 24(%%rsi), %%r8\n"
    "adcq $0, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += n3 * c0 */
    "movq %8, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += n2 * c1 */
    "movq %9, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += n1 */
    "addq %%r12, %%r8\n"
    "adcq $0, %%r9\n"
    "adcq $0, %%r10\n"
    /* extract m3 */
    "movq %%r8, %q3\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r10,r8) += n3 * c1 */
    "movq %9, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += n2 */
    "addq %%r13, %%r9\n"
    "adcq $0, %%r10\n"
    "adcq $0, %%r8\n"
    /* extract m4 */
    "movq %%r9, %q4\n"
    /* (r10,r8) += n3 */
    "addq %%r14, %%r10\n"
    "adcq $0, %%r8\n"
    /* extract m5 */
    "movq %%r10, %q5\n"
    /* extract m6 */
    "movq %%r8, %q6\n"
    : "=&g"(m0), "=&g"(m1), "=&g"(m2), "=g"(m3), "=g"(m4), "=g"(m5), "=g"(m6)
    : "S"(l), "i"(SECP256K1_N_C_0), "i"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "cc");

    /* Reduce 385 bits into 258. */
    __asm__ __volatile__(
    /* Preload */
    "movq %q9, %%r11\n"
    "movq %q10, %%r12\n"
    "movq %q11, %%r13\n"
    /* Initialize (r8,r9,r10) */
    "movq %q5, %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /* (r8,r9) += m4 * c0 */
    "movq %12, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /* extract p0 */
    "movq %%r8, %q0\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r10) += m1 */
    "addq %q6, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r9,r10,r8) += m5 * c0 */
    "movq %12, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += m4 * c1 */
    "movq %13, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* extract p1 */
    "movq %%r9, %q1\n"
    "xorq %%r9, %%r9\n"
    /* (r10,r8,r9) += m2 */
    "addq %q7, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += m6 * c0 */
    "movq %12, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += m5 * c1 */
    "movq %13, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += m4 */
    "addq %%r11, %%r10\n"
    "adcq $0, %%r8\n"
    "adcq $0, %%r9\n"
    /* extract p2 */
    "movq %%r10, %q2\n"
    /* (r8,r9) += m3 */
    "addq %q8, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r8,r9) += m6 * c1 */
    "movq %13, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /* (r8,r9) += m5 */
    "addq %%r12, %%r8\n"
    "adcq $0, %%r9\n"
    /* extract p3 */
    "movq %%r8, %q3\n"
    /* (r9) += m6 */
    "addq %%r13, %%r9\n"
    /* extract p4 */
    "movq %%r9, %q4\n"
    : "=&g"(p0), "=&g"(p1), "=&g"(p2), "=g"(p3), "=g"(p4)
    : "g"(m0), "g"(m1), "g"(m2), "g"(m3), "g"(m4), "g"(m5), "g"(m6), "i"(SECP256K1_N_C_0), "i"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "cc");

    /* Reduce 258 bits into 256. */
    __asm__ __volatile__(
    /* Preload */
    "movq %q5, %%r10\n"
    /* (rax,rdx) = p4 * c0 */
    "movq %7, %%rax\n"
    "mulq %%r10\n"
    /* (rax,rdx) += p0 */
    "addq %q1, %%rax\n"
    "adcq $0, %%rdx\n"
    /* extract r0 */
    "movq %%rax, 0(%q6)\n"
    /* Move to (r8,r9) */
    "movq %%rdx, %%r8\n"
    "xorq %%r9, %%r9\n"
    /* (r8,r9) += p1 */
    "addq %q2, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r8,r9) += p4 * c1 */
    "movq %8, %%rax\n"
    "mulq %%r10\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    /* Extract r1 */
    "movq %%r8, 8(%q6)\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r8) += p4 */
    "addq %%r10, %%r9\n"
    "adcq $0, %%r8\n"
    /* (r9,r8) += p2 */
    "addq %q3, %%r9\n"
    "adcq $0, %%r8\n"
    /* Extract r2 */
    "movq %%r9, 16(%q6)\n"
    "xorq %%r9, %%r9\n"
    /* (r8,r9) += p3 */
    "addq %q4, %%r8\n"
    "adcq $0, %%r9\n"
    /* Extract r3 */
    "movq %%r8, 24(%q6)\n"
    /* Extract c */
    "movq %%r9, %q0\n"
    : "=g"(c)
    : "g"(p0), "g"(p1), "g"(p2), "g"(p3), "g"(p4), "D"(r), "i"(SECP256K1_N_C_0), "i"(SECP256K1_N_C_1)
    : "rax", "rdx", "r8", "r9", "r10", "cc", "memory");
#else
    rustsecp256k1_v0_9_1_uint128 c128;
    uint64_t c, c0, c1, c2;
    uint64_t n0 = l[4], n1 = l[5], n2 = l[6], n3 = l[7];
    uint64_t m0, m1, m2, m3, m4, m5;
    uint32_t m6;
    uint64_t p0, p1, p2, p3;
    uint32_t p4;

    /* Reduce 512 bits into 385. */
    /* m[0..6] = l[0..3] + n[0..3] * SECP256K1_N_C. */
    c0 = l[0]; c1 = 0; c2 = 0;
    muladd_fast(n0, SECP256K1_N_C_0);
    extract_fast(m0);
    sumadd_fast(l[1]);
    muladd(n1, SECP256K1_N_C_0);
    muladd(n0, SECP256K1_N_C_1);
    extract(m1);
    sumadd(l[2]);
    muladd(n2, SECP256K1_N_C_0);
    muladd(n1, SECP256K1_N_C_1);
    sumadd(n0);
    extract(m2);
    sumadd(l[3]);
    muladd(n3, SECP256K1_N_C_0);
    muladd(n2, SECP256K1_N_C_1);
    sumadd(n1);
    extract(m3);
    muladd(n3, SECP256K1_N_C_1);
    sumadd(n2);
    extract(m4);
    sumadd_fast(n3);
    extract_fast(m5);
    VERIFY_CHECK(c0 <= 1);
    m6 = c0;

    /* Reduce 385 bits into 258. */
    /* p[0..4] = m[0..3] + m[4..6] * SECP256K1_N_C. */
    c0 = m0; c1 = 0; c2 = 0;
    muladd_fast(m4, SECP256K1_N_C_0);
    extract_fast(p0);
    sumadd_fast(m1);
    muladd(m5, SECP256K1_N_C_0);
    muladd(m4, SECP256K1_N_C_1);
    extract(p1);
    sumadd(m2);
    muladd(m6, SECP256K1_N_C_0);
    muladd(m5, SECP256K1_N_C_1);
    sumadd(m4);
    extract(p2);
    sumadd_fast(m3);
    muladd_fast(m6, SECP256K1_N_C_1);
    sumadd_fast(m5);
    extract_fast(p3);
    p4 = c0 + m6;
    VERIFY_CHECK(p4 <= 2);

    /* Reduce 258 bits into 256. */
    /* r[0..3] = p[0..3] + p[4] * SECP256K1_N_C. */
    rustsecp256k1_v0_9_1_u128_from_u64(&c128, p0);
    rustsecp256k1_v0_9_1_u128_accum_mul(&c128, SECP256K1_N_C_0, p4);
    r->d[0] = rustsecp256k1_v0_9_1_u128_to_u64(&c128); rustsecp256k1_v0_9_1_u128_rshift(&c128, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&c128, p1);
    rustsecp256k1_v0_9_1_u128_accum_mul(&c128, SECP256K1_N_C_1, p4);
    r->d[1] = rustsecp256k1_v0_9_1_u128_to_u64(&c128); rustsecp256k1_v0_9_1_u128_rshift(&c128, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&c128, p2);
    rustsecp256k1_v0_9_1_u128_accum_u64(&c128, p4);
    r->d[2] = rustsecp256k1_v0_9_1_u128_to_u64(&c128); rustsecp256k1_v0_9_1_u128_rshift(&c128, 64);
    rustsecp256k1_v0_9_1_u128_accum_u64(&c128, p3);
    r->d[3] = rustsecp256k1_v0_9_1_u128_to_u64(&c128);
    c = rustsecp256k1_v0_9_1_u128_hi_u64(&c128);
#endif

    /* Final reduction of r. */
    rustsecp256k1_v0_9_1_scalar_reduce(r, c + rustsecp256k1_v0_9_1_scalar_check_overflow(r));
}

static void rustsecp256k1_v0_9_1_scalar_mul_512(uint64_t l[8], const rustsecp256k1_v0_9_1_scalar *a, const rustsecp256k1_v0_9_1_scalar *b) {
#ifdef USE_ASM_X86_64
    const uint64_t *pb = b->d;
    __asm__ __volatile__(
    /* Preload */
    "movq 0(%%rdi), %%r15\n"
    "movq 8(%%rdi), %%rbx\n"
    "movq 16(%%rdi), %%rcx\n"
    "movq 0(%%rdx), %%r11\n"
    "movq 8(%%rdx), %%r12\n"
    "movq 16(%%rdx), %%r13\n"
    "movq 24(%%rdx), %%r14\n"
    /* (rax,rdx) = a0 * b0 */
    "movq %%r15, %%rax\n"
    "mulq %%r11\n"
    /* Extract l0 */
    "movq %%rax, 0(%%rsi)\n"
    /* (r8,r9,r10) = (rdx) */
    "movq %%rdx, %%r8\n"
    "xorq %%r9, %%r9\n"
    "xorq %%r10, %%r10\n"
    /* (r8,r9,r10) += a0 * b1 */
    "movq %%r15, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += a1 * b0 */
    "movq %%rbx, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* Extract l1 */
    "movq %%r8, 8(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r10,r8) += a0 * b2 */
    "movq %%r15, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += a1 * b1 */
    "movq %%rbx, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += a2 * b0 */
    "movq %%rcx, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* Extract l2 */
    "movq %%r9, 16(%%rsi)\n"
    "xorq %%r9, %%r9\n"
    /* (r10,r8,r9) += a0 * b3 */
    "movq %%r15, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* Preload a3 */
    "movq 24(%%rdi), %%r15\n"
    /* (r10,r8,r9) += a1 * b2 */
    "movq %%rbx, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += a2 * b1 */
    "movq %%rcx, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* (r10,r8,r9) += a3 * b0 */
    "movq %%r15, %%rax\n"
    "mulq %%r11\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    "adcq $0, %%r9\n"
    /* Extract l3 */
    "movq %%r10, 24(%%rsi)\n"
    "xorq %%r10, %%r10\n"
    /* (r8,r9,r10) += a1 * b3 */
    "movq %%rbx, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += a2 * b2 */
    "movq %%rcx, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* (r8,r9,r10) += a3 * b1 */
    "movq %%r15, %%rax\n"
    "mulq %%r12\n"
    "addq %%rax, %%r8\n"
    "adcq %%rdx, %%r9\n"
    "adcq $0, %%r10\n"
    /* Extract l4 */
    "movq %%r8, 32(%%rsi)\n"
    "xorq %%r8, %%r8\n"
    /* (r9,r10,r8) += a2 * b3 */
    "movq %%rcx, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* (r9,r10,r8) += a3 * b2 */
    "movq %%r15, %%rax\n"
    "mulq %%r13\n"
    "addq %%rax, %%r9\n"
    "adcq %%rdx, %%r10\n"
    "adcq $0, %%r8\n"
    /* Extract l5 */
    "movq %%r9, 40(%%rsi)\n"
    /* (r10,r8) += a3 * b3 */
    "movq %%r15, %%rax\n"
    "mulq %%r14\n"
    "addq %%rax, %%r10\n"
    "adcq %%rdx, %%r8\n"
    /* Extract l6 */
    "movq %%r10, 48(%%rsi)\n"
    /* Extract l7 */
    "movq %%r8, 56(%%rsi)\n"
    : "+d"(pb)
    : "S"(l), "D"(a->d)
    : "rax", "rbx", "rcx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "cc", "memory");
#else
    /* 160 bit accumulator. */
    uint64_t c0 = 0, c1 = 0;
    uint32_t c2 = 0;

    /* l[0..7] = a[0..3] * b[0..3]. */
    muladd_fast(a->d[0], b->d[0]);
    extract_fast(l[0]);
    muladd(a->d[0], b->d[1]);
    muladd(a->d[1], b->d[0]);
    extract(l[1]);
    muladd(a->d[0], b->d[2]);
    muladd(a->d[1], b->d[1]);
    muladd(a->d[2], b->d[0]);
    extract(l[2]);
    muladd(a->d[0], b->d[3]);
    muladd(a->d[1], b->d[2]);
    muladd(a->d[2], b->d[1]);
    muladd(a->d[3], b->d[0]);
    extract(l[3]);
    muladd(a->d[1], b->d[3]);
    muladd(a->d[2], b->d[2]);
    muladd(a->d[3], b->d[1]);
    extract(l[4]);
    muladd(a->d[2], b->d[3]);
    muladd(a->d[3], b->d[2]);
    extract(l[5]);
    muladd_fast(a->d[3], b->d[3]);
    extract_fast(l[6]);
    VERIFY_CHECK(c1 == 0);
    l[7] = c0;
#endif
}

#undef sumadd
#undef sumadd_fast
#undef muladd
#undef muladd_fast
#undef extract
#undef extract_fast

static void rustsecp256k1_v0_9_1_scalar_mul(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *a, const rustsecp256k1_v0_9_1_scalar *b) {
    uint64_t l[8];
    rustsecp256k1_v0_9_1_scalar_verify(a);
    rustsecp256k1_v0_9_1_scalar_verify(b);

    rustsecp256k1_v0_9_1_scalar_mul_512(l, a, b);
    rustsecp256k1_v0_9_1_scalar_reduce_512(r, l);

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

static int rustsecp256k1_v0_9_1_scalar_shr_int(rustsecp256k1_v0_9_1_scalar *r, int n) {
    int ret;
    rustsecp256k1_v0_9_1_scalar_verify(r);
    VERIFY_CHECK(n > 0);
    VERIFY_CHECK(n < 16);

    ret = r->d[0] & ((1 << n) - 1);
    r->d[0] = (r->d[0] >> n) + (r->d[1] << (64 - n));
    r->d[1] = (r->d[1] >> n) + (r->d[2] << (64 - n));
    r->d[2] = (r->d[2] >> n) + (r->d[3] << (64 - n));
    r->d[3] = (r->d[3] >> n);

    rustsecp256k1_v0_9_1_scalar_verify(r);
    return ret;
}

static void rustsecp256k1_v0_9_1_scalar_split_128(rustsecp256k1_v0_9_1_scalar *r1, rustsecp256k1_v0_9_1_scalar *r2, const rustsecp256k1_v0_9_1_scalar *k) {
    rustsecp256k1_v0_9_1_scalar_verify(k);

    r1->d[0] = k->d[0];
    r1->d[1] = k->d[1];
    r1->d[2] = 0;
    r1->d[3] = 0;
    r2->d[0] = k->d[2];
    r2->d[1] = k->d[3];
    r2->d[2] = 0;
    r2->d[3] = 0;

    rustsecp256k1_v0_9_1_scalar_verify(r1);
    rustsecp256k1_v0_9_1_scalar_verify(r2);
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_eq(const rustsecp256k1_v0_9_1_scalar *a, const rustsecp256k1_v0_9_1_scalar *b) {
    rustsecp256k1_v0_9_1_scalar_verify(a);
    rustsecp256k1_v0_9_1_scalar_verify(b);

    return ((a->d[0] ^ b->d[0]) | (a->d[1] ^ b->d[1]) | (a->d[2] ^ b->d[2]) | (a->d[3] ^ b->d[3])) == 0;
}

SECP256K1_INLINE static void rustsecp256k1_v0_9_1_scalar_mul_shift_var(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *a, const rustsecp256k1_v0_9_1_scalar *b, unsigned int shift) {
    uint64_t l[8];
    unsigned int shiftlimbs;
    unsigned int shiftlow;
    unsigned int shifthigh;
    rustsecp256k1_v0_9_1_scalar_verify(a);
    rustsecp256k1_v0_9_1_scalar_verify(b);
    VERIFY_CHECK(shift >= 256);

    rustsecp256k1_v0_9_1_scalar_mul_512(l, a, b);
    shiftlimbs = shift >> 6;
    shiftlow = shift & 0x3F;
    shifthigh = 64 - shiftlow;
    r->d[0] = shift < 512 ? (l[0 + shiftlimbs] >> shiftlow | (shift < 448 && shiftlow ? (l[1 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[1] = shift < 448 ? (l[1 + shiftlimbs] >> shiftlow | (shift < 384 && shiftlow ? (l[2 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[2] = shift < 384 ? (l[2 + shiftlimbs] >> shiftlow | (shift < 320 && shiftlow ? (l[3 + shiftlimbs] << shifthigh) : 0)) : 0;
    r->d[3] = shift < 320 ? (l[3 + shiftlimbs] >> shiftlow) : 0;
    rustsecp256k1_v0_9_1_scalar_cadd_bit(r, 0, (l[(shift - 1) >> 6] >> ((shift - 1) & 0x3f)) & 1);

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

static SECP256K1_INLINE void rustsecp256k1_v0_9_1_scalar_cmov(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *a, int flag) {
    uint64_t mask0, mask1;
    volatile int vflag = flag;
    rustsecp256k1_v0_9_1_scalar_verify(a);
    SECP256K1_CHECKMEM_CHECK_VERIFY(r->d, sizeof(r->d));

    mask0 = vflag + ~((uint64_t)0);
    mask1 = ~mask0;
    r->d[0] = (r->d[0] & mask0) | (a->d[0] & mask1);
    r->d[1] = (r->d[1] & mask0) | (a->d[1] & mask1);
    r->d[2] = (r->d[2] & mask0) | (a->d[2] & mask1);
    r->d[3] = (r->d[3] & mask0) | (a->d[3] & mask1);

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

static void rustsecp256k1_v0_9_1_scalar_from_signed62(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_modinv64_signed62 *a) {
    const uint64_t a0 = a->v[0], a1 = a->v[1], a2 = a->v[2], a3 = a->v[3], a4 = a->v[4];

    /* The output from rustsecp256k1_v0_9_1_modinv64{_var} should be normalized to range [0,modulus), and
     * have limbs in [0,2^62). The modulus is < 2^256, so the top limb must be below 2^(256-62*4).
     */
    VERIFY_CHECK(a0 >> 62 == 0);
    VERIFY_CHECK(a1 >> 62 == 0);
    VERIFY_CHECK(a2 >> 62 == 0);
    VERIFY_CHECK(a3 >> 62 == 0);
    VERIFY_CHECK(a4 >> 8 == 0);

    r->d[0] = a0      | a1 << 62;
    r->d[1] = a1 >> 2 | a2 << 60;
    r->d[2] = a2 >> 4 | a3 << 58;
    r->d[3] = a3 >> 6 | a4 << 56;

    rustsecp256k1_v0_9_1_scalar_verify(r);
}

static void rustsecp256k1_v0_9_1_scalar_to_signed62(rustsecp256k1_v0_9_1_modinv64_signed62 *r, const rustsecp256k1_v0_9_1_scalar *a) {
    const uint64_t M62 = UINT64_MAX >> 2;
    const uint64_t a0 = a->d[0], a1 = a->d[1], a2 = a->d[2], a3 = a->d[3];
    rustsecp256k1_v0_9_1_scalar_verify(a);

    r->v[0] =  a0                   & M62;
    r->v[1] = (a0 >> 62 | a1 <<  2) & M62;
    r->v[2] = (a1 >> 60 | a2 <<  4) & M62;
    r->v[3] = (a2 >> 58 | a3 <<  6) & M62;
    r->v[4] =  a3 >> 56;
}

static const rustsecp256k1_v0_9_1_modinv64_modinfo rustsecp256k1_v0_9_1_const_modinfo_scalar = {
    {{0x3FD25E8CD0364141LL, 0x2ABB739ABD2280EELL, -0x15LL, 0, 256}},
    0x34F20099AA774EC1LL
};

static void rustsecp256k1_v0_9_1_scalar_inverse(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *x) {
    rustsecp256k1_v0_9_1_modinv64_signed62 s;
#ifdef VERIFY
    int zero_in = rustsecp256k1_v0_9_1_scalar_is_zero(x);
#endif
    rustsecp256k1_v0_9_1_scalar_verify(x);

    rustsecp256k1_v0_9_1_scalar_to_signed62(&s, x);
    rustsecp256k1_v0_9_1_modinv64(&s, &rustsecp256k1_v0_9_1_const_modinfo_scalar);
    rustsecp256k1_v0_9_1_scalar_from_signed62(r, &s);

    rustsecp256k1_v0_9_1_scalar_verify(r);
#ifdef VERIFY
    VERIFY_CHECK(rustsecp256k1_v0_9_1_scalar_is_zero(r) == zero_in);
#endif
}

static void rustsecp256k1_v0_9_1_scalar_inverse_var(rustsecp256k1_v0_9_1_scalar *r, const rustsecp256k1_v0_9_1_scalar *x) {
    rustsecp256k1_v0_9_1_modinv64_signed62 s;
#ifdef VERIFY
    int zero_in = rustsecp256k1_v0_9_1_scalar_is_zero(x);
#endif
    rustsecp256k1_v0_9_1_scalar_verify(x);

    rustsecp256k1_v0_9_1_scalar_to_signed62(&s, x);
    rustsecp256k1_v0_9_1_modinv64_var(&s, &rustsecp256k1_v0_9_1_const_modinfo_scalar);
    rustsecp256k1_v0_9_1_scalar_from_signed62(r, &s);

    rustsecp256k1_v0_9_1_scalar_verify(r);
#ifdef VERIFY
    VERIFY_CHECK(rustsecp256k1_v0_9_1_scalar_is_zero(r) == zero_in);
#endif
}

SECP256K1_INLINE static int rustsecp256k1_v0_9_1_scalar_is_even(const rustsecp256k1_v0_9_1_scalar *a) {
    rustsecp256k1_v0_9_1_scalar_verify(a);

    return !(a->d[0] & 1);
}

#endif /* SECP256K1_SCALAR_REPR_IMPL_H */
