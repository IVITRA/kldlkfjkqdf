#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include "../core/router.h"
#include "../core/memory_manager.h"
#include "../core/context_protocol.h"
#include "../core/memory_pool.h"
#include "../formats/asm_format.h"

using namespace asm_core;

// Critical Check #2: Dequantization Speed Test
TEST(Performance, DequantizationSpeed) {
    const size_t NUM_PARAMS = 10'000'000; // 10M parameters
    
    std::vector<float> original(NUM_PARAMS, 0.5f);
    size_t compressed_size = asm_format::TernaryStorage::compressed_size(NUM_PARAMS);
    std::vector<int8_t> compressed(compressed_size);
    std::vector<float> decompressed(NUM_PARAMS);
    
    // Compress once
    asm_format::TernaryStorage::compress(original.data(), compressed.data(), NUM_PARAMS);
    
    // Benchmark: decompress 1000 experts
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        asm_format::TernaryStorage::decompress(
            compressed.data(), decompressed.data(), NUM_PARAMS, nullptr);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Must be less than 50ms for 1000 experts
    EXPECT_LT(duration.count(), 50) << "Dequantization too slow: " << duration.count() << "ms";
}

// Critical Check #3: Cross-Domain Context Bridge
TEST(Context, CrossDomainUnderstanding) {
    InterlinguaTranslator translator;
    
    // Physics expert talks about "gravity affects time"
    ThoughtPassport physics_passport;
    physics_passport.confidence = 0.95f;
    physics_passport.source_expert_id = 1;
    
    // Fill with physics-related latent vector
    for (int i = 0; i < 128; ++i) {
        physics_passport.intent[i] = static_cast<float>(i % 10) / 10.0f;
    }
    
    // Translate to Philosophy domain
    auto philosophy_passport = translator.translate(
        physics_passport, Domain::PHYSICS, Domain::PHILOSOPHY);
    
    // Verify context is preserved (confidence should remain > 0)
    EXPECT_GT(philosophy_passport.confidence, 0.0f);
    
    // Should maintain reasonable confidence after translation
    EXPECT_GT(philosophy_passport.confidence, 0.5f);
}

// Critical Check #4: LRU Cache Eviction
TEST(Memory, LRUEviction) {
    auto& mm = HierarchicalMemoryManager::instance();
    
    // Register 50 experts
    for (int i = 0; i < 50; ++i) {
        ColdExpert expert;
        expert.id = "expert_" + std::to_string(i);
        expert.domain = "test";
        expert.file_offset = i * 2048;
        expert.compressed_size = 1024;
        expert.usage_frequency = 1.0f;
        expert.last_accessed_token = i;
        std::fill(expert.centroid.begin(), expert.centroid.end(), 0.0f);
        
        mm.register_expert(expert);
    }
    
    EXPECT_EQ(mm.get_total_registered_experts(), 50);
    
    // Note: Actual eviction happens during load_blocking
    // which requires disk I/O. This test verifies registration works.
}

// Test: Fallback Expert System
TEST(Router, FallbackExpert) {
    HierarchicalRouter router;
    
    // Build graph with 100 experts
    std::vector<std::array<float, 128>> centroids(100);
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 128; ++j) {
            centroids[i][j] = static_cast<float>(i + j) / 256.0f;
        }
    }
    
    router.build_graph(centroids);
    
    // Set expert 0 as fallback (generalist)
    router.set_fallback_expert(0);
    
    // Search with a query
    float query[128];
    for (int i = 0; i < 128; ++i) {
        query[i] = static_cast<float>(i) / 256.0f;
    }
    
    float confidence = 0.0f;
    auto result = router.search_nearest(query, 5, &confidence);
    
    // Should return results
    EXPECT_FALSE(result.empty());
    
    // Confidence should be calculated
    EXPECT_GT(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

// Test: Memory Pool Pre-allocation
TEST(Memory, PoolAllocation) {
    ExpertMemoryPool pool;
    
    // Should start empty
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.used_slots, 0);
    EXPECT_EQ(stats.total_slots, 20);
    
    // Allocate 5 experts (2MB each)
    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate(i, 2 * 1024 * 1024);
        EXPECT_NE(ptr, nullptr);
        EXPECT_TRUE(pool.is_loaded(i));
    }
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.used_slots, 5);
    EXPECT_EQ(stats.used_capacity, 5 * 2 * 1024 * 1024);
    
    // Deallocate
    pool.deallocate(0);
    EXPECT_FALSE(pool.is_loaded(0));
}

// Test: Expert File Size Verification
TEST(Format, ExpertFileSize) {
    // Calculate expected size for 10M params
    size_t num_params = 10'000'000;
    size_t ternary_size = asm_format::TernaryStorage::compressed_size(num_params);
    size_t total_size = asm_format::ExpertHeader::HEADER_SIZE + ternary_size;
    
    // Should be approximately 2MB
    size_t expected_mb = 2;
    size_t actual_mb = total_size / (1024 * 1024);
    
    // Allow some tolerance (should be < 3MB)
    EXPECT_LT(actual_mb, 3) << "Expert file too large: " << actual_mb << "MB";
}

// Test: Router Performance
TEST(Router, SearchPerformance) {
    HierarchicalRouter router;
    
    // Build graph with 1000 experts
    const int NUM_EXPERTS = 1000;
    std::vector<std::array<float, 128>> centroids(NUM_EXPERTS);
    
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (int i = 0; i < NUM_EXPERTS; ++i) {
        for (int j = 0; j < 128; ++j) {
            centroids[i][j] = dist(gen);
        }
    }
    
    router.build_graph(centroids);
    
    // Benchmark 100 searches
    float query[128];
    for (int i = 0; i < 128; ++i) {
        query[i] = dist(gen);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        auto result = router.search_nearest(query, 10);
        EXPECT_EQ(result.size(), 10);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avg_ms = duration.count() / 100.0;
    
    // Average search should be < 0.5ms
    EXPECT_LT(avg_ms, 0.5) << "Router too slow: " << avg_ms << "ms per search";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
