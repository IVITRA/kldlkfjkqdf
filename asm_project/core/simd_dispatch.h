#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

// SIMD Architecture Detection
#if defined(__AVX2__) || defined(__AVX__)
    #define ASM_SIMD_AVX2 1
    #include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define ASM_SIMD_NEON 1
    #include <arm_neon.h>
#else
    #define ASM_SIMD_SCALAR 1
#endif

namespace asm_core {

// SIMD-Optimized Distance Calculation with Automatic Fallback
class SimdDispatch {
public:
    // Euclidean distance squared (faster, no sqrt needed for comparison)
    static float euclidean_distance_squared(const float* a, const float* b, int dimensions) {
#if ASM_SIMD_AVX2
        return avx2_distance_squared(a, b, dimensions);
#elif ASM_SIMD_NEON
        return neon_distance_squared(a, b, dimensions);
#else
        return scalar_distance_squared(a, b, dimensions);
#endif
    }
    
    // Full Euclidean distance
    static float euclidean_distance(const float* a, const float* b, int dimensions) {
        return std::sqrt(euclidean_distance_squared(a, b, dimensions));
    }
    
    // Dot product
    static float dot_product(const float* a, const float* b, int dimensions) {
#if ASM_SIMD_AVX2
        return avx2_dot_product(a, b, dimensions);
#elif ASM_SIMD_NEON
        return neon_dot_product(a, b, dimensions);
#else
        return scalar_dot_product(a, b, dimensions);
#endif
    }

private:
    // AVX2 Implementation (Intel/AMD)
#if ASM_SIMD_AVX2
    static float avx2_distance_squared(const float* a, const float* b, int dimensions) {
        __m256 sum = _mm256_setzero_ps();
        
        for (int i = 0; i < dimensions; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 diff = _mm256_sub_ps(va, vb);
            sum = _mm256_fmadd_ps(diff, diff, sum);
        }
        
        // Horizontal sum
        __m128 low = _mm256_castps256_ps128(sum);
        __m128 high = _mm256_extractf128_ps(sum, 1);
        __m128 sum128 = _mm_add_ps(low, high);
        
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);
        
        return _mm_cvtss_f32(sum128);
    }
    
    static float avx2_dot_product(const float* a, const float* b, int dimensions) {
        __m256 sum = _mm256_setzero_ps();
        
        for (int i = 0; i < dimensions; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            sum = _mm256_fmadd_ps(va, vb, sum);
        }
        
        __m128 low = _mm256_castps256_ps128(sum);
        __m128 high = _mm256_extractf128_ps(sum, 1);
        __m128 sum128 = _mm_add_ps(low, high);
        
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);
        
        return _mm_cvtss_f32(sum128);
    }
#endif
    
    // NEON Implementation (ARM)
#if ASM_SIMD_NEON
    static float neon_distance_squared(const float* a, const float* b, int dimensions) {
        float32x4_t sum = vdupq_n_f32(0.0f);
        
        for (int i = 0; i < dimensions; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            float32x4_t diff = vsubq_f32(va, vb);
            sum = vfmaq_f32(sum, diff, diff);
        }
        
        // Horizontal sum
        float32x2_t sum_low = vget_low_f32(sum);
        float32x2_t sum_high = vget_high_f32(sum);
        float32x2_t sum_total = vpadd_f32(sum_low, sum_high);
        sum_total = vpadd_f32(sum_total, sum_total);
        
        return vget_lane_f32(sum_total, 0);
    }
    
    static float neon_dot_product(const float* a, const float* b, int dimensions) {
        float32x4_t sum = vdupq_n_f32(0.0f);
        
        for (int i = 0; i < dimensions; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            sum = vfmaq_f32(sum, va, vb);
        }
        
        float32x2_t sum_low = vget_low_f32(sum);
        float32x2_t sum_high = vget_high_f32(sum);
        float32x2_t sum_total = vpadd_f32(sum_low, sum_high);
        sum_total = vpadd_f32(sum_total, sum_total);
        
        return vget_lane_f32(sum_total, 0);
    }
#endif
    
    // Scalar Fallback (works everywhere)
    static float scalar_distance_squared(const float* a, const float* b, int dimensions) {
        float sum = 0.0f;
        for (int i = 0; i < dimensions; ++i) {
            float diff = a[i] - b[i];
            sum += diff * diff;
        }
        return sum;
    }
    
    static float scalar_dot_product(const float* a, const float* b, int dimensions) {
        float sum = 0.0f;
        for (int i = 0; i < dimensions; ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }
    
public:
    // Get SIMD backend name
    static const char* get_backend_name() {
#if ASM_SIMD_AVX2
        return "AVX2 (Intel/AMD)";
#elif ASM_SIMD_NEON
        return "NEON (ARM)";
#else
        return "Scalar (Fallback)";
#endif
    }
    
    // Check if SIMD is available
    static bool has_simd() {
#if ASM_SIMD_AVX2 || ASM_SIMD_NEON
        return true;
#else
        return false;
#endif
    }
};

} // namespace asm_core
