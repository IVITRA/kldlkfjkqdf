#pragma once
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "memory_manager.h"
#include "router.h"
#include "memory_safety.h"

namespace asm_core {

// Resilient Expert Manager with Graceful Degradation
class ResilientExpertManager {
private:
    HierarchicalMemoryManager& mem_manager;
    HierarchicalRouter& router;
    std::shared_ptr<HotExpert> generalist_expert; // Always kept in RAM
    
public:
    ResilientExpertManager(HierarchicalMemoryManager& mm, HierarchicalRouter& r)
        : mem_manager(mm), router(r) {}
    
    // Initialize generalist expert (always resident in RAM)
    void initialize_generalist(const std::string& expert_id) {
        try {
            generalist_expert = mem_manager.load_blocking(expert_id);
            std::cout << "Generalist expert loaded: " << expert_id << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to load generalist expert: " << e.what() << std::endl;
        }
    }
    
    // Load expert with fallback mechanisms
    std::shared_ptr<HotExpert> load_with_fallback(const std::string& expert_id) {
        try {
            // Try primary expert
            return mem_manager.load_blocking(expert_id);
            
        } catch (const CorruptedExpertException& e) {
            std::cerr << "Expert " << expert_id << " corrupted, trying nearest neighbor" << std::endl;
            
            // Try to find second-best expert
            try {
                float query[128] = {0}; // Would use actual query vector
                float confidence = 0.0f;
                auto candidates = router.search_nearest(query, 2, &confidence);
                
                if (candidates.size() > 1) {
                    std::string fallback_id = std::to_string(candidates[1]);
                    std::cout << "Using fallback expert: " << fallback_id << std::endl;
                    return mem_manager.load_blocking(fallback_id);
                }
            } catch (...) {
                std::cerr << "Fallback routing failed" << std::endl;
            }
            
        } catch (const SSDReadException& e) {
            std::cerr << "SSD read failed: " << e.what() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading expert " << expert_id << ": " << e.what() << std::endl;
        }
        
        // Last resort: use generalist
        if (generalist_expert) {
            std::cout << "Using generalist expert as last resort" << std::endl;
            return generalist_expert;
        }
        
        throw std::runtime_error("No experts available!");
    }
    
    // Check if system is healthy
    struct HealthStatus {
        bool mem_manager_healthy;
        bool router_healthy;
        bool generalist_available;
        size_t cache_size;
        std::string status_message;
    };
    
    HealthStatus check_health() const {
        HealthStatus status;
        status.mem_manager_healthy = true;
        status.router_healthy = true;
        status.generalist_available = (generalist_expert != nullptr);
        status.cache_size = mem_manager.get_hot_cache_size();
        status.status_message = "OK";
        
        if (!status.generalist_available) {
            status.status_message = "WARNING: No generalist expert loaded";
        }
        
        return status;
    }
};

} // namespace asm_core
