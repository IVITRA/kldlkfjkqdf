#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <random>
#include <atomic>
#include <cstdint>
#include <memory>
#include <cstring>

#ifdef __AVX2__
#include <immintrin.h> // AVX2 intrinsics
#endif

namespace asm_core {

// Node في الـ HNSW Graph
struct HNSWNode {
    uint32_t expert_id;
    // centroid مُحاذى للـ Cache Line (64 bytes)
    alignas(64) std::array<float, 128> centroid;
    
    // Connections في الطبقات المختلفة
    // Layer 0 (الأسفل): كثير الاتصالات (~32)
    // Layer >0 (الأعلى): قليل الاتصالات (~4)
    std::vector<uint32_t> neighbors[4]; // 4 طبقات كافية لـ 50K
    uint8_t max_layer;
};

class HierarchicalRouter {
private:
    // جميع الـ Centroids مُرتبة في memory contiguous للـ SIMD
    alignas(64) float centroids_linear[50000][128]; // 25.6 MB (يكفي في L3 Cache!)
    
    // الـ Graph
    std::vector<HNSWNode> nodes;
    uint32_t entry_point = 0; // البوابة العليا
    
    // الـ Random Generator للبحث العشوائي
    std::mt19937 rng;
    
    // Parameters
    static constexpr int M = 16; // Max connections per layer
    static constexpr int ef_construction = 200;
    static constexpr int ef_search = 50;
    
    size_t num_experts = 0;
    
    // Fallback expert for low-confidence routing
    uint32_t fallback_expert_id = 0;
    bool has_fallback = false;

public:
    HierarchicalRouter() : rng(42) {}
    
    // البناء: يتم مرة واحدة عند الـ Loading
    void build_graph(const std::vector<std::array<float, 128>>& expert_centroids);
    
    // البحث: يجب أن يكون أقل من 0.5ms
    std::vector<uint32_t> search_nearest(const float* query_vec, int top_k = 10, 
                                         float* confidence = nullptr);
    
    // Add a single expert to the router
    void add_expert(uint32_t expert_id, const std::array<float, 128>& centroid);
    
    // Set fallback expert (generalist)
    void set_fallback_expert(uint32_t expert_id) {
        fallback_expert_id = expert_id;
        has_fallback = true;
    }
    
private:
    // **السر**: AVX2 SIMD Distance Calculation
    // تحسب مسافة Euclidean لـ 8 أبعاد في آن واحد
    inline float avx2_distance(const float* a, const float* b) {
#ifdef __AVX2__
        __m256 sum = _mm256_setzero_ps();
        
        for (int i = 0; i < 128; i += 8) {
            __m256 va = _mm256_load_ps(a + i);
            __m256 vb = _mm256_load_ps(b + i);
            __m256 diff = _mm256_sub_ps(va, vb);
            sum = _mm256_fmadd_ps(diff, diff, sum); // FMA: أسرع
        }
        
        // Horizontal sum
        __m256 hsum = _mm256_hadd_ps(sum, sum);
        hsum = _mm256_hadd_ps(hsum, hsum);
        float result[8];
        _mm256_storeu_ps(result, hsum);
        return std::sqrt(result[0] + result[4]);
#else
        // Fallback for non-AVX2 systems
        float sum = 0.0f;
        for (int i = 0; i < 128; ++i) {
            float diff = a[i] - b[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
#endif
    }
    
    // Greedy Search في طبقة معينة
    std::vector<uint32_t> search_layer(const float* query, uint32_t entry, 
                                       int ef, int layer);
    
    // Select neighbors for HNSW
    std::vector<uint32_t> select_neighbors(const std::vector<uint32_t>& candidates, int m);
    
    // Calculate distance between two experts
    float distance_between(uint32_t id1, uint32_t id2);
};

// **التحسين الإضافي**: Router Cache للاستعلامات المتشابهة
class RouterCache {
    // Circular buffer للـ Recently Used Queries
    static constexpr size_t CACHE_SIZE = 128;
    struct CacheEntry {
        std::array<float, 128> query;
        std::vector<uint32_t> result;
        bool valid = false;
    };
    
    std::array<CacheEntry, CACHE_SIZE> cache;
    std::atomic<size_t> cache_idx{0};
    
public:
    bool lookup(const float* query, std::vector<uint32_t>& result) {
        // Simple linear search for similar queries
        for (size_t i = 0; i < CACHE_SIZE; ++i) {
            if (!cache[i].valid) continue;
            
            // Check if query is similar (within threshold)
            float dist = 0.0f;
            for (int j = 0; j < 128; ++j) {
                float diff = query[j] - cache[i].query[j];
                dist += diff * diff;
            }
            
            if (dist < 0.01f) { // Threshold for "similar"
                result = cache[i].result;
                return true;
            }
        }
        return false;
    }
    
    void store(const float* query, const std::vector<uint32_t>& result) {
        size_t idx = cache_idx.fetch_add(1) % CACHE_SIZE;
        
        std::memcpy(cache[idx].query.data(), query, sizeof(cache[idx].query));
        cache[idx].result = result;
        cache[idx].valid = true;
    }
};

} // namespace asm_core
