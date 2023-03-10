#pragma once
#include <assert.h>
#include <complex>

// NOTE: Assumes arrays are aligned
// Multiply and accumulate vector of complex floats with vector of floats

static inline
std::complex<float> c32_f32_cum_mul_scalar(const std::complex<float>* x0, const float* x1, const int N) {
    auto y = std::complex<float>(0,0);
    for (int i = 0; i < N; i++) {
        y += x0[i] * x1[i];
    }
    return y;
}

// TODO: Modify code to support ARM platforms like Raspberry PI using NEON
#include <immintrin.h>
#include "simd_config.h"
#include "data_packing.h"
#include "c32_cum_sum.h"

#if defined(_DSP_SSSE3)
static inline
std::complex<float> c32_f32_cum_mul_ssse3(const std::complex<float>* x0, const float* x1, const int N)
{
    // 128bits = 16bytes = 4*4bytes
    constexpr int K = 4;
    const int M = N/K;
    const int N_vector = M*K;
    const int N_remain = N-N_vector;

    // [3 2 1 0] -> [3 3 2 2]
    const uint8_t PERMUTE_UPPER = 0b11111010;
    // [3 2 1 0] -> [1 1 0 0]
    const uint8_t PERMUTE_LOWER = 0b01010000;

    cpx128_t v_sum;
    v_sum.ps = _mm_set1_ps(0.0f);

    for (int i = 0; i < N_vector; i+=K) {
        // [c0 c1]
        __m128 a0 = _mm_loadu_ps(reinterpret_cast<const float*>(&x0[i]));
        // [c2 c3]
        __m128 a1 = _mm_loadu_ps(reinterpret_cast<const float*>(&x0[i + K/2]));

        // [a0 a1 a2 a3]
        __m128 b0 = _mm_loadu_ps(&x1[i]);
        // [a2 a2 a3 a3]
        __m128 b1 = _mm_shuffle_ps(b0, b0, PERMUTE_UPPER);
        // [a0 a0 a1 a1]
        b0 = _mm_shuffle_ps(b0, b0, PERMUTE_LOWER);

        // multiply accumulate
        #if !defined(_DSP_FMA)
        v_sum.ps = _mm_add_ps(_mm_mul_ps(a0, b0), v_sum.ps);
        v_sum.ps = _mm_add_ps(_mm_mul_ps(a1, b1), v_sum.ps);
        #else
        v_sum.ps = _mm_fmadd_ps(a0, b0, v_sum.ps);
        v_sum.ps = _mm_fmadd_ps(a1, b1, v_sum.ps);
        #endif
    }

    auto y = std::complex<float>(0,0);
    y += c32_cum_sum_ssse3(v_sum);
    y += c32_f32_cum_mul_scalar(&x0[N_vector], &x1[N_vector], N_remain);
    return y;
}
#endif

#if defined(_DSP_AVX2)
static inline
std::complex<float> c32_f32_cum_mul_avx2(const std::complex<float>* x0, const float* x1, const int N)
{
    // 256bits = 32bytes = 4*8bytes
    constexpr int K = 4;
    const int M = N/K;
    const int N_vector = M*K;
    const int N_remain = N-N_vector;

    // [3 2 1 0] -> [3 3 2 2]
    const uint8_t PERMUTE_UPPER = 0b11111010;
    // [3 2 1 0] -> [1 1 0 0]
    const uint8_t PERMUTE_LOWER = 0b01010000;

    cpx256_t v_sum;
    v_sum.ps = _mm256_set1_ps(0.0f);

    for (int i = 0; i < N_vector; i+=K) {
        // [c0 c1 c2 c3]
        __m256 a0 = _mm256_loadu_ps(reinterpret_cast<const float*>(&x0[i]));

        // [a0 a1 a2 a3]
        __m128 b0 = _mm_loadu_ps(&x1[i]);
        // [a2 a2 a3 a3]
        __m128 b1 = _mm_permute_ps(b0, PERMUTE_LOWER);
        // [a0 a0 a1 a1]
        b0 = _mm_permute_ps(b0, PERMUTE_UPPER);

        // [a0 a0 a1 a1 a2 a2 a3 a3]
        __m256 a1 = _mm256_set_m128(b0, b1);

        // multiply accumulate
        #if !defined(_DSP_FMA)
        v_sum.ps = _mm256_add_ps(_mm256_mul_ps(a0, a1), v_sum.ps);
        #else
        v_sum.ps = _mm256_fmadd_ps(a0, a1, v_sum.ps);
        #endif
    }

    auto y = std::complex<float>(0,0);
    y += c32_cum_sum_avx2(v_sum);
    y += c32_f32_cum_mul_scalar(&x0[N_vector], &x1[N_vector], N_remain);
    return y;
}
#endif

inline static 
std::complex<float> c32_f32_cum_mul_auto(const std::complex<float>* x0, const float* x1, const int N) {
    #if defined(_DSP_AVX2)
    return c32_f32_cum_mul_avx2(x0, x1, N);
    #elif defined(_DSP_SSSE3)
    return c32_f32_cum_mul_ssse3(x0, x1, N);
    #else
    return c32_f32_cum_mul_scalar(x0, x1, N);
    #endif
}
