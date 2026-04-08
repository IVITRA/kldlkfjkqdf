#include <gtest/gtest.h>
#include "../core/router.h"

using namespace asm_core;

TEST(RouterTest, BuildAndSearch) {
    HierarchicalRouter router;
    
    // Create dummy centroids for 100 experts
    std::vector<std::array<float, 128>> centroids(100);
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 128; ++j) {
            centroids[i][j] = static_cast<float>(i + j) / 256.0f;
        }
    }
    
    router.build_graph(centroids);
    
    // Search for nearest
    float query[128];
    for (int i = 0; i < 128; ++i) {
        query[i] = static_cast<float>(i) / 256.0f;
    }
    
    auto result = router.search_nearest(query, 5);
    
    EXPECT_EQ(result.size(), 5);
}

TEST(RouterTest, CacheHit) {
    RouterCache cache;
    
    float query[128] = {0};
    std::vector<uint32_t> expected = {1, 2, 3, 4, 5};
    
    // Store in cache
    cache.store(query, expected);
    
    // Lookup
    std::vector<uint32_t> result;
    bool found = cache.lookup(query, result);
    
    EXPECT_TRUE(found);
    EXPECT_EQ(result, expected);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
