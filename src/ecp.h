/*
Copyright 2014-2015 Coinfloor LTD.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gmp.h>

#if GMP_LIMB_BITS == 64
#define MP_LIMB_C(lo, hi) (UINT64_C(lo) | UINT64_C(hi) << 32)
#elif GMP_LIMB_BITS == 32
#define MP_LIMB_C(lo, hi) UINT32_C(lo), UINT32_C(hi)
#else
#error "unsupported limb size"
#endif

#define MP_NLIMBS(n) (((n) + sizeof(mp_limb_t) - 1) / sizeof(mp_limb_t))


extern const mp_limb_t secp224k1_p[MP_NLIMBS(29)], secp224k1_a[MP_NLIMBS(29)], secp224k1_G[3][MP_NLIMBS(29)], secp224k1_n[MP_NLIMBS(29)];

extern const mp_limb_t secp256k1_p[MP_NLIMBS(32)], secp256k1_a[MP_NLIMBS(32)], secp256k1_G[3][MP_NLIMBS(32)], secp256k1_n[MP_NLIMBS(32)];


void bytes_to_mpn(mp_limb_t mpn[], const uint8_t bytes[], size_t n);

void mpn_to_bytes(uint8_t bytes[], const mp_limb_t mpn[], size_t n);

void ecp_pubkey(mp_limb_t Q[], const mp_limb_t p[], const mp_limb_t a[], const mp_limb_t G[], const mp_limb_t d[], size_t l);

void ecp_sign(mp_limb_t r[], mp_limb_t s[], const mp_limb_t p[], const mp_limb_t a[], const mp_limb_t G[], const mp_limb_t n[], const mp_limb_t d[], const mp_limb_t z[], size_t l);

bool ecp_verify(const mp_limb_t p[], const mp_limb_t a[], const mp_limb_t G[], const mp_limb_t n[], const mp_limb_t Q[], const mp_limb_t z[], const mp_limb_t r[], const mp_limb_t s[], size_t l);
