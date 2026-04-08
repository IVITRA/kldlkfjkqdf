#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <string>
#include <array>
#include <atomic>
#include <mio/mmap.hpp>
#include "../formats/asm_format.h"

namespace asm_core {

struct ColdExpert {
    std::string id;
    std::string domain;
    size_t file_offset;
    size_t compressed_size;
    std::array<float, 128> centroid;
    float usage_frequency; // للـ LRU الاحتمالي
    int last_accessed_token; // للـ LRU
};

struct HotExpert {
    std::string id;
    // الأوزان في صيغة 4-bit (uint8_t يحمل قيمتين)
    std::vector<uint8_t> weights_q4;
    std::vector<float> scales;
    size_t last_accessed;
    std::atomic<bool> is_decompressing{false}; // علم للـ Async
    size_t num_params;
};

class HierarchicalMemoryManager {
private:
    // LRU Cache
    std::list<std::string> lru_list;
    std::unordered_map<std::string, std::pair<std::shared_ptr<HotExpert>, 
        std::list<std::string>::iterator>> hot_cache;
    
    // Shared Mutex للقراءات المتعددة (C++17)
    mutable std::shared_mutex cache_mutex;
    
    // الـ Cold Registry (metadata فقط، صغير جداً)
    std::unordered_map<std::string, ColdExpert> cold_registry;
    
    // Memory-mapped files للـ SSD
    std::unique_ptr<mio::mmap_source> ssd_mmap;
    
    // الحدود
    static constexpr size_t MAX_HOT_EXPERTS = 20; // ~200MB
    static constexpr size_t MAX_RAM_USAGE = 2ULL * 1024 * 1024 * 1024; // 2GB للنظام
    
    size_t current_ram_usage = 0;
    std::atomic<size_t> access_counter{0};

public:
    // Singleton Pattern
    static HierarchicalMemoryManager& instance() {
        static HierarchicalMemoryManager instance;
        return instance;
    }
    
    // Async Loading Interface
    struct LoadTicket {
        std::string expert_id;
        std::shared_ptr<HotExpert> target;
        std::atomic<bool> ready{false};
        std::atomic<float> progress{0.0f};
    };
    
    // Initialize with experts file
    bool initialize(const std::string& experts_file_path);
    
    // Register a cold expert (metadata only)
    void register_expert(const ColdExpert& expert);
    
    // Async request to load expert
    std::shared_ptr<LoadTicket> request_load_async(const std::string& expert_id);
    
    // Get expert if already in cache
    std::shared_ptr<HotExpert> get_if_available(const std::string& expert_id);
    
    // Force immediate load (للـ Critical Path)
    std::shared_ptr<HotExpert> load_blocking(const std::string& expert_id);
    
    // Background eviction (يشتغل في Thread منفصل)
    void evict_lru_if_needed();
    
    // Get statistics
    size_t get_hot_cache_size() const;
    size_t get_current_ram_usage() const;
    size_t get_total_registered_experts() const;
    
    // Delete copy constructor and assignment
    HierarchicalMemoryManager(const HierarchicalMemoryManager&) = delete;
    HierarchicalMemoryManager& operator=(const HierarchicalMemoryManager&) = delete;
    
private:
    HierarchicalMemoryManager() = default;
    ~HierarchicalMemoryManager() = default;
    
    void decompress_worker(std::shared_ptr<LoadTicket> ticket);
    size_t calculate_expert_ram_size(const ColdExpert& cold);
    void move_to_front(const std::string& expert_id);
    std::shared_ptr<HotExpert> load_from_disk(const std::string& expert_id);
};

} // namespace asm_core
