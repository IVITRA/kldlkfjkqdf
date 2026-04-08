#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <random>
#include <mutex>
#include "router.h"

namespace asm_core {

// Deterministic Router for Session Consistency
class DeterministicRouter {
private:
    HierarchicalRouter& base_router;
    std::mt19937 rng;
    bool deterministic_mode = false;
    
    // User preference memory
    struct UserPreferences {
        std::vector<uint32_t> preferred_experts;
        std::map<std::string, uint32_t> domain_preferences;
        int session_count = 0;
    };
    
    std::map<std::string, UserPreferences> user_memory;
    std::mutex memory_mutex;
    
public:
    DeterministicRouter(HierarchicalRouter& router, uint32_t seed = 42)
        : base_router(router), rng(seed) {}
    
    // Enable/disable deterministic mode
    void set_deterministic(bool enable) {
        deterministic_mode = enable;
        if (enable) {
            rng.seed(42); // Fixed seed for reproducibility
        }
    }
    
    // Search with consistency guarantees
    std::vector<uint32_t> search_consistent(
        const float* query_vec,
        int top_k,
        const std::string& user_id = "",
        float* confidence = nullptr) {
        
        std::vector<uint32_t> results;
        
        // Get base results from router
        float base_confidence = 0.0f;
        results = base_router.search_nearest(query_vec, top_k, &base_confidence);
        
        if (confidence) {
            *confidence = base_confidence;
        }
        
        // Apply user preferences if available
        if (!user_id.empty()) {
            results = apply_user_preferences(results, user_id);
        }
        
        // In deterministic mode, always return same results for same query
        if (deterministic_mode) {
            std::sort(results.begin(), results.end());
        }
        
        return results;
    }
    
    // Store user feedback for future routing
    void store_user_feedback(
        const std::string& user_id,
        uint32_t selected_expert,
        const std::string& domain) {
        
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        auto& prefs = user_memory[user_id];
        prefs.session_count++;
        
        // Add to preferred experts (with decay)
        prefs.preferred_experts.push_back(selected_expert);
        
        // Keep only last 100 preferences
        if (prefs.preferred_experts.size() > 100) {
            prefs.preferred_experts.erase(prefs.preferred_experts.begin());
        }
        
        // Update domain preferences
        prefs.domain_preferences[domain] = selected_expert;
    }
    
    // Get user's preferred experts for a domain
    std::vector<uint32_t> get_user_preferred_experts(
        const std::string& user_id,
        const std::string& domain = "") {
        
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        auto it = user_memory.find(user_id);
        if (it == user_memory.end()) {
            return {};
        }
        
        const auto& prefs = it->second;
        
        if (!domain.empty()) {
            auto domain_it = prefs.domain_preferences.find(domain);
            if (domain_it != prefs.domain_preferences.end()) {
                return {domain_it->second};
            }
        }
        
        return prefs.preferred_experts;
    }
    
    // Clear user memory (privacy)
    void clear_user_memory(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(memory_mutex);
        user_memory.erase(user_id);
    }
    
    // Get session consistency score
    float get_consistency_score(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(memory_mutex);
        
        auto it = user_memory.find(user_id);
        if (it == user_memory.end()) {
            return 1.0f; // No history, perfectly consistent
        }
        
        const auto& prefs = it->second;
        if (prefs.preferred_experts.empty()) {
            return 1.0f;
        }
        
        // Calculate consistency based on how often same experts are chosen
        std::map<uint32_t, int> expert_counts;
        for (uint32_t expert_id : prefs.preferred_experts) {
            expert_counts[expert_id]++;
        }
        
        float max_count = 0;
        for (const auto& pair : expert_counts) {
            if (pair.second > max_count) {
                max_count = pair.second;
            }
        }
        
        return max_count / prefs.preferred_experts.size();
    }
    
private:
    // Apply user preferences to routing results
    std::vector<uint32_t> apply_user_preferences(
        const std::vector<uint32_t>& base_results,
        const std::string& user_id) {
        
        auto prefs_it = user_memory.find(user_id);
        if (prefs_it == user_memory.end()) {
            return base_results;
        }
        
        const auto& prefs = prefs_it->second;
        if (prefs.preferred_experts.empty()) {
            return base_results;
        }
        
        // Boost preferred experts
        std::vector<uint32_t> results = base_results;
        
        // Sort by preference frequency
        std::sort(results.begin(), results.end(),
            [&prefs](uint32_t a, uint32_t b) {
                int count_a = 0, count_b = 0;
                for (uint32_t pref : prefs.preferred_experts) {
                    if (pref == a) count_a++;
                    if (pref == b) count_b++;
                }
                return count_a > count_b;
            });
        
        return results;
    }
};

} // namespace asm_core
