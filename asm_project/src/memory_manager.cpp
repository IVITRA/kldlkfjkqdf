#include "memory_manager.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>
#include <thread>

namespace asm_core {

bool HierarchicalMemoryManager::initialize(const std::string& experts_file_path) {
    try {
        ssd_mmap = std::make_unique<mio::mmap_source>(experts_file_path);
        
        if (!ssd_mmap->is_open()) {
            std::cerr << "Failed to open experts file: " << experts_file_path << std::endl;
            return false;
        }
        
        // Parse the file and register all experts
        size_t offset = 0;
        while (offset + asm_format::ExpertHeader::HEADER_SIZE <= ssd_mmap->size()) {
            const auto* header = reinterpret_cast<const asm_format::ExpertHeader*>(
                ssd_mmap->data() + offset);
            
            if (!header->validate_magic()) {
                break; // End of valid experts
            }
            
            ColdExpert cold;
            cold.id = std::string(header->expert_id);
            cold.domain = std::string(header->domain);
            cold.file_offset = offset;
            cold.compressed_size = header->weights_size_compressed;
            cold.usage_frequency = 1.0f;
            cold.last_accessed_token = 0;
            std::memcpy(cold.centroid.data(), header->router_centroid, sizeof(cold.centroid));
            
            cold_registry[cold.id] = cold;
            
            // Move to next expert
            offset = header->metadata_offset > 0 ? 
                     static_cast<size_t>(header->metadata_offset) : 
                     offset + asm_format::ExpertHeader::HEADER_SIZE + header->weights_size_compressed;
        }
        
        std::cout << "Loaded " << cold_registry.size() << " experts from disk" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error initializing memory manager: " << e.what() << std::endl;
        return false;
    }
}

void HierarchicalMemoryManager::register_expert(const ColdExpert& expert) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    cold_registry[expert.id] = expert;
}

std::shared_ptr<HierarchicalMemoryManager::LoadTicket> 
HierarchicalMemoryManager::request_load_async(const std::string& expert_id) {
    auto ticket = std::make_shared<LoadTicket>();
    ticket->expert_id = expert_id;
    
    // Check if already in cache
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex);
        auto it = hot_cache.find(expert_id);
        if (it != hot_cache.end()) {
            ticket->target = it->second.first;
            ticket->ready = true;
            ticket->progress = 1.0f;
            return ticket;
        }
    }
    
    // Load in background thread
    std::thread([this, ticket]() {
        this->decompress_worker(ticket);
    }).detach();
    
    return ticket;
}

std::shared_ptr<HotExpert> HierarchicalMemoryManager::get_if_available(const std::string& expert_id) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    
    auto it = hot_cache.find(expert_id);
    if (it != hot_cache.end()) {
        // Update LRU
        lru_list.erase(it->second.second);
        lru_list.push_front(expert_id);
        it->second.second = lru_list.begin();
        it->second.first->last_accessed = access_counter.fetch_add(1);
        
        return it->second.first;
    }
    
    return nullptr;
}

std::shared_ptr<HotExpert> HierarchicalMemoryManager::load_blocking(const std::string& expert_id) {
    // Check cache first
    if (auto cached = get_if_available(expert_id)) {
        return cached;
    }
    
    // Load from disk
    auto expert = load_from_disk(expert_id);
    if (!expert) {
        return nullptr;
    }
    
    // Calculate expert size
    size_t expert_size = expert->weights_q4.size() + expert->scales.size() * sizeof(float);
    
    // CRITICAL FIX #1: RAM Guard - Check if expert fits in RAM limit
    if (expert_size > MAX_RAM_USAGE) {
        std::cerr << "❌ Expert too large for RAM limit: " << expert_size << " bytes" << std::endl;
        return nullptr;
    }
    
    // Add to cache with RAM Guard
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex);
        
        // CRITICAL: Evict LRU experts until we have enough RAM
        while (current_ram_usage + expert_size > MAX_RAM_USAGE && !lru_list.empty()) {
            evict_lru_expert(); // Immediate eviction if memory is full
        }
        
        // Also respect MAX_HOT_EXPERTS limit
        while (hot_cache.size() >= MAX_HOT_EXPERTS && !lru_list.empty()) {
            evict_lru_expert();
        }
        
        // Add new expert
        lru_list.push_front(expert_id);
        hot_cache[expert_id] = {expert, lru_list.begin()};
        current_ram_usage += expert_size;
    }
    
    return expert;
}

void HierarchicalMemoryManager::evict_lru_if_needed() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    
    while ((hot_cache.size() > MAX_HOT_EXPERTS || current_ram_usage > MAX_RAM_USAGE) 
           && !lru_list.empty()) {
        evict_lru_expert();
    }
}

void HierarchicalMemoryManager::evict_lru_expert() {
    // Must be called with lock already held
    if (lru_list.empty()) return;
    
    std::string lru_id = lru_list.back();
    lru_list.pop_back();
    
    auto it = hot_cache.find(lru_id);
    if (it != hot_cache.end()) {
        current_ram_usage -= it->second.first->weights_q4.size();
        current_ram_usage -= it->second.first->scales.size() * sizeof(float);
        hot_cache.erase(it);
    }
}

size_t HierarchicalMemoryManager::get_hot_cache_size() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    return hot_cache.size();
}

size_t HierarchicalMemoryManager::get_current_ram_usage() const {
    return current_ram_usage.load();
}

size_t HierarchicalMemoryManager::get_total_registered_experts() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    return cold_registry.size();
}

void HierarchicalMemoryManager::decompress_worker(std::shared_ptr<LoadTicket> ticket) {
    try {
        ticket->progress = 0.1f;
        
        // Load from disk
        auto expert = load_from_disk(ticket->expert_id);
        if (!expert) {
            ticket->progress = 0.0f;
            return;
        }
        
        ticket->progress = 0.8f;
        
        // CRITICAL FIX #1: Add to cache with RAM Guard
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex);
            
            // Calculate expert size
            size_t expert_size = expert->weights_q4.size() + expert->scales.size() * sizeof(float);
            
            // Check if expert fits in RAM
            if (expert_size > MAX_RAM_USAGE) {
                std::cerr << "Expert too large for RAM limit!" << std::endl;
                ticket->progress = 0.0f;
                return;
            }
            
            // Evict LRU experts until we have enough RAM
            while (current_ram_usage + expert_size > MAX_RAM_USAGE && !lru_list.empty()) {
                evict_lru_expert();
            }
            
            // Also respect MAX_HOT_EXPERTS limit
            while (hot_cache.size() >= MAX_HOT_EXPERTS && !lru_list.empty()) {
                evict_lru_expert();
            }
            
            ticket->target = expert;
            lru_list.push_front(ticket->expert_id);
            hot_cache[ticket->expert_id] = {expert, lru_list.begin()};
            current_ram_usage += expert_size;
        }
        
        ticket->progress = 1.0f;
        ticket->ready = true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error decompressing expert " << ticket->expert_id 
                  << ": " << e.what() << std::endl;
        ticket->progress = 0.0f;
    }
}

size_t HierarchicalMemoryManager::calculate_expert_ram_size(const ColdExpert& cold) {
    // Approximate: uncompressed weights (FP32) + scales
    return cold.compressed_size * 8 + 1024; // Rough estimate
}

void HierarchicalMemoryManager::move_to_front(const std::string& expert_id) {
    // Must be called with lock held
    auto it = hot_cache.find(expert_id);
    if (it != hot_cache.end()) {
        lru_list.erase(it->second.second);
        lru_list.push_front(expert_id);
        it->second.second = lru_list.begin();
    }
}

std::shared_ptr<HotExpert> HierarchicalMemoryManager::load_from_disk(const std::string& expert_id) {
    ColdExpert cold;
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex);
        auto it = cold_registry.find(expert_id);
        if (it == cold_registry.end()) {
            std::cerr << "Expert not found: " << expert_id << std::endl;
            return nullptr;
        }
        cold = it->second;
    }
    
    if (!ssd_mmap || !ssd_mmap->is_open()) {
        std::cerr << "SSD mmap not initialized" << std::endl;
        return nullptr;
    }
    
    auto expert = std::make_shared<HotExpert>();
    expert->id = expert_id;
    expert->last_accessed = access_counter.fetch_add(1);
    expert->is_decompressing = true;
    
    // Read header
    if (cold.file_offset + asm_format::ExpertHeader::HEADER_SIZE > ssd_mmap->size()) {
        return nullptr;
    }
    
    const auto* header = reinterpret_cast<const asm_format::ExpertHeader*>(
        ssd_mmap->data() + cold.file_offset);
    
    expert->num_params = header->num_params;
    
    // Read compressed weights
    size_t weights_start = cold.file_offset + asm_format::ExpertHeader::HEADER_SIZE;
    if (weights_start + cold.compressed_size > ssd_mmap->size()) {
        return nullptr;
    }
    
    const uint8_t* compressed_data = ssd_mmap->data() + weights_start;
    expert->weights_q4.assign(compressed_data, compressed_data + cold.compressed_size);
    
    // For now, use simple scales (in real implementation, read from file)
    size_t num_scales = (header->num_params + 15) / 16; // One scale per 16 params
    expert->scales.resize(num_scales, 1.0f);
    
    expert->is_decompressing = false;
    
    return expert;
}

// CRITICAL FIX #7: Prefetching - Load experts in background before they're needed
void HierarchicalMemoryManager::prefetch_experts(const std::vector<std::string>& expert_ids) {
    for (const auto& id : expert_ids) {
        // Check if already in cache
        if (!get_if_available(id)) {
            // Request async load without waiting
            request_load_async(id);
        }
    }
}

} // namespace asm_core
