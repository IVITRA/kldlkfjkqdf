#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <iostream>
#include <iomanip>

namespace asm_core {

class Telemetry {
private:
    struct Metrics {
        // Router metrics
        double router_avg_latency_ms = 0.0;
        double router_max_latency_ms = 0.0;
        uint64_t router_query_count = 0;
        
        // Memory manager metrics
        size_t hot_cache_size = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        size_t total_ram_usage = 0;
        
        // I/O metrics
        double ssd_read_throughput_mbs = 0.0;
        double expert_load_avg_ms = 0.0;
        uint64_t total_experts_loaded = 0;
        
        // Context metrics
        double context_coherence_score = 0.0;
        uint64_t context_switches = 0;
        
        // Inference metrics
        double tokens_per_second = 0.0;
        uint64_t total_tokens_generated = 0;
        
        // Timing
        std::chrono::steady_clock::time_point start_time;
    };
    
    Metrics metrics;
    mutable std::mutex mtx;
    
public:
    Telemetry() : metrics{} {
        metrics.start_time = std::chrono::steady_clock::now();
    }
    
    // Record router latency
    void record_router_query(double latency_ms) {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.router_query_count++;
        
        // Running average
        double alpha = 0.1;
        metrics.router_avg_latency_ms = 
            alpha * latency_ms + (1.0 - alpha) * metrics.router_avg_latency_ms;
        
        metrics.router_max_latency_ms = 
            std::max(metrics.router_max_latency_ms, latency_ms);
    }
    
    // Record cache hit/miss
    void record_cache_hit() {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.cache_hits++;
    }
    
    void record_cache_miss() {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.cache_misses++;
    }
    
    // Record expert load
    void record_expert_load(double load_time_ms, size_t ram_usage) {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.total_experts_loaded++;
        
        double alpha = 0.1;
        metrics.expert_load_avg_ms = 
            alpha * load_time_ms + (1.0 - alpha) * metrics.expert_load_avg_ms;
        
        metrics.total_ram_usage = ram_usage;
    }
    
    // Update context metrics
    void update_context_coherence(double score) {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.context_coherence_score = score;
    }
    
    void record_context_switch() {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.context_switches++;
    }
    
    // Record token generation
    void record_tokens(uint64_t count, double elapsed_seconds) {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.total_tokens_generated += count;
        if (elapsed_seconds > 0) {
            metrics.tokens_per_second = count / elapsed_seconds;
        }
    }
    
    // Update hot cache size
    void update_hot_cache_size(size_t size) {
        std::lock_guard<std::mutex> lock(mtx);
        metrics.hot_cache_size = size;
    }
    
    // Print full diagnostic report
    void print_diagnostic_report() const {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics.start_time).count();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "           ASM TELEMETRY REPORT" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        std::cout << "\n⏱️  Uptime: " << uptime << " seconds\n" << std::endl;
        
        std::cout << "🧠 Router Performance:" << std::endl;
        std::cout << "  Avg Latency:      " << std::fixed << std::setprecision(2) 
                  << metrics.router_avg_latency_ms << " ms" << std::endl;
        std::cout << "  Max Latency:      " << metrics.router_max_latency_ms << " ms" << std::endl;
        std::cout << "  Total Queries:    " << metrics.router_query_count << std::endl;
        
        std::cout << "\n💾 Memory Manager:" << std::endl;
        std::cout << "  Hot Cache Size:   " << metrics.hot_cache_size << " experts" << std::endl;
        std::cout << "  RAM Usage:        " << format_bytes(metrics.total_ram_usage) << std::endl;
        
        double hit_rate = 0.0;
        if (metrics.cache_hits + metrics.cache_misses > 0) {
            hit_rate = 100.0 * metrics.cache_hits / (metrics.cache_hits + metrics.cache_misses);
        }
        std::cout << "  Cache Hit Rate:   " << std::fixed << std::setprecision(1) 
                  << hit_rate << "%" << std::endl;
        
        std::cout << "\n💿 I/O Performance:" << std::endl;
        std::cout << "  Avg Load Time:    " << std::fixed << std::setprecision(2) 
                  << metrics.expert_load_avg_ms << " ms" << std::endl;
        std::cout << "  Total Loaded:     " << metrics.total_experts_loaded << " experts" << std::endl;
        
        std::cout << "\n🔗 Context Bridge:" << std::endl;
        std::cout << "  Coherence Score:  " << std::fixed << std::setprecision(2) 
                  << metrics.context_coherence_score << std::endl;
        std::cout << "  Context Switches: " << metrics.context_switches << std::endl;
        
        std::cout << "\n⚡ Inference:" << std::endl;
        std::cout << "  Tokens/sec:       " << std::fixed << std::setprecision(1) 
                  << metrics.tokens_per_second << std::endl;
        std::cout << "  Total Tokens:     " << metrics.total_tokens_generated << std::endl;
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        
        // Performance verdict
        std::cout << "\n📊 Performance Verdict:" << std::endl;
        if (metrics.router_avg_latency_ms < 0.5) {
            std::cout << "  ✓ Router: EXCELLENT (< 0.5ms)" << std::endl;
        } else if (metrics.router_avg_latency_ms < 1.0) {
            std::cout << "  ⚠ Router: GOOD (< 1ms)" << std::endl;
        } else {
            std::cout << "  ✗ Router: NEEDS OPTIMIZATION" << std::endl;
        }
        
        if (metrics.expert_load_avg_ms < 50.0) {
            std::cout << "  ✓ Expert Load: EXCELLENT (< 50ms)" << std::endl;
        } else if (metrics.expert_load_avg_ms < 100.0) {
            std::cout << "  ⚠ Expert Load: ACCEPTABLE (< 100ms)" << std::endl;
        } else {
            std::cout << "  ✗ Expert Load: TOO SLOW" << std::endl;
        }
        
        if (hit_rate > 80.0) {
            std::cout << "  ✓ Cache Hit Rate: EXCELLENT (> 80%)" << std::endl;
        } else {
            std::cout << "  ⚠ Cache Hit Rate: COULD BE BETTER" << std::endl;
        }
        
        std::cout << std::string(60, '=') << "\n" << std::endl;
    }
    
private:
    static std::string format_bytes(size_t bytes) {
        if (bytes >= 1024ULL * 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
        } else if (bytes >= 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024)) + " MB";
        } else if (bytes >= 1024) {
            return std::to_string(bytes / 1024) + " KB";
        }
        return std::to_string(bytes) + " bytes";
    }
};

// Global telemetry instance
inline Telemetry& get_telemetry() {
    static Telemetry instance;
    return instance;
}

} // namespace asm_core
