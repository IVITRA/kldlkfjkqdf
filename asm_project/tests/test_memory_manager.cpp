#include <gtest/gtest.h>
#include "../core/memory_manager.h"

using namespace asm_core;

TEST(MemoryManagerTest, SingletonInstance) {
    auto& instance1 = HierarchicalMemoryManager::instance();
    auto& instance2 = HierarchicalMemoryManager::instance();
    
    EXPECT_EQ(&instance1, &instance2);
}

TEST(MemoryManagerTest, RegisterExpert) {
    auto& manager = HierarchicalMemoryManager::instance();
    
    ColdExpert expert;
    expert.id = "test_expert_1";
    expert.domain = "medical";
    expert.file_offset = 0;
    expert.compressed_size = 1024;
    expert.usage_frequency = 1.0f;
    expert.last_accessed_token = 0;
    
    manager.register_expert(expert);
    
    EXPECT_EQ(manager.get_total_registered_experts(), 1);
}

TEST(MemoryManagerTest, LoadNonExistentExpert) {
    auto& manager = HierarchicalMemoryManager::instance();
    
    // Should return nullptr for non-existent expert
    auto expert = manager.load_blocking("non_existent");
    EXPECT_EQ(expert, nullptr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
