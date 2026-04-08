#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>
#include <stdexcept>

namespace asm_core {

// Memory Pool for Expert weights - avoids malloc during runtime
class ExpertMemoryPool {
private:
    static constexpr size_t MAX_EXPERTS_IN_POOL = 20;
    static constexpr size_t MAX_EXPERT_SIZE = 10 * 1024 * 1024; // 10MB per expert
    
    // Pre-allocated aligned memory pool
    alignas(64) std::array<uint8_t, MAX_EXPERTS_IN_POOL * MAX_EXPERT_SIZE> pool;
    std::array<bool, MAX_EXPERTS_IN_POOL> slot_used;
    std::array<size_t, MAX_EXPERTS_IN_POOL> slot_sizes;
    
public:
    ExpertMemoryPool() {
        slot_used.fill(false);
        slot_sizes.fill(0);
    }
    
    // Allocate memory for an expert from the pool
    void* allocate(size_t expert_idx, size_t size) {
        if (expert_idx >= MAX_EXPERTS_IN_POOL) {
            throw std::runtime_error("Expert index out of pool range");
        }
        
        if (size > MAX_EXPERT_SIZE) {
            throw std::runtime_error("Expert size exceeds pool limit");
        }
        
        if (slot_used[expert_idx]) {
            // Already allocated, return existing
            return pool.data() + (expert_idx * MAX_EXPERT_SIZE);
        }
        
        slot_used[expert_idx] = true;
        slot_sizes[expert_idx] = size;
        
        return pool.data() + (expert_idx * MAX_EXPERT_SIZE);
    }
    
    // Free a slot
    void deallocate(size_t expert_idx) {
        if (expert_idx >= MAX_EXPERTS_IN_POOL) {
            throw std::runtime_error("Expert index out of pool range");
        }
        
        slot_used[expert_idx] = false;
        slot_sizes[expert_idx] = 0;
    }
    
    // Get pointer to expert data
    void* get(size_t expert_idx) {
        if (expert_idx >= MAX_EXPERTS_IN_POOL || !slot_used[expert_idx]) {
            return nullptr;
        }
        return pool.data() + (expert_idx * MAX_EXPERT_SIZE);
    }
    
    // Check if expert is loaded
    bool is_loaded(size_t expert_idx) const {
        return expert_idx < MAX_EXPERTS_IN_POOL && slot_used[expert_idx];
    }
    
    // Get pool statistics
    struct PoolStats {
        size_t total_slots;
        size_t used_slots;
        size_t total_capacity;
        size_t used_capacity;
        size_t fragmentation;
    };
    
    PoolStats get_stats() const {
        PoolStats stats;
        stats.total_slots = MAX_EXPERTS_IN_POOL;
        stats.used_slots = 0;
        stats.total_capacity = MAX_EXPERTS_IN_POOL * MAX_EXPERT_SIZE;
        stats.used_capacity = 0;
        
        for (size_t i = 0; i < MAX_EXPERTS_IN_POOL; ++i) {
            if (slot_used[i]) {
                stats.used_slots++;
                stats.used_capacity += slot_sizes[i];
            }
        }
        
        stats.fragmentation = stats.total_capacity - stats.used_capacity;
        return stats;
    }
    
    // Reset pool
    void reset() {
        slot_used.fill(false);
        slot_sizes.fill(0);
    }
};

// Stack-based allocator for small allocations
class StackAllocator {
private:
    static constexpr size_t STACK_SIZE = 50 * 1024 * 1024; // 50MB stack
    alignas(64) std::array<uint8_t, STACK_SIZE> memory;
    size_t offset = 0;
    
public:
    void* allocate(size_t size, size_t alignment = 64) {
        // Align offset
        size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
        
        if (aligned_offset + size > STACK_SIZE) {
            throw std::runtime_error("Stack allocator overflow");
        }
        
        void* ptr = memory.data() + aligned_offset;
        offset = aligned_offset + size;
        
        return ptr;
    }
    
    void reset() {
        offset = 0;
    }
    
    size_t get_used() const { return offset; }
    size_t get_capacity() const { return STACK_SIZE; }
};

} // namespace asm_core
