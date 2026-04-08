#pragma once
#include <span>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <iostream>

// Enable safety checks in all builds for production safety
#define ASM_SAFETY_CHECKS 1

#ifdef DEBUG
    #define ASM_THREAD_GUARD std::scoped_lock lock(mutex_);
    #define ASM_DEBUG_ONLY(x) x
#else
    #define ASM_THREAD_GUARD
    #define ASM_DEBUG_ONLY(x)
#endif

namespace asm_core {

// Memory Safety Utilities
class MemorySafety {
public:
    // Safe span creation with bounds checking
    template<typename T>
    static std::span<T> create_span(T* data, size_t offset, size_t length, size_t total_size) {
#if ASM_SAFETY_CHECKS
        if (data == nullptr) {
            throw std::invalid_argument("Null pointer in create_span");
        }
        if (offset + length > total_size) {
            throw std::out_of_range(
                "Weight access violation: offset=" + std::to_string(offset) + 
                ", length=" + std::to_string(length) + 
                ", total=" + std::to_string(total_size));
        }
#endif
        return std::span<T>(data + offset, length);
    }
    
    // Safe array access
    template<typename T>
    static T& safe_access(T* array, size_t index, size_t max_size, const std::string& context) {
#if ASM_SAFETY_CHECKS
        if (array == nullptr) {
            throw std::invalid_argument("Null array access in " + context);
        }
        if (index >= max_size) {
            throw std::out_of_range(
                "Array bounds violation in " + context + 
                ": index=" + std::to_string(index) + 
                ", max=" + std::to_string(max_size));
        }
#endif
        return array[index];
    }
    
    // Validate expert ID
    static void validate_expert_id(uint32_t expert_id, uint32_t max_id, const std::string& context) {
#if ASM_SAFETY_CHECKS
        if (expert_id > max_id) {
            throw std::out_of_range(
                "Invalid expert ID in " + context + 
                ": id=" + std::to_string(expert_id) + 
                ", max=" + std::to_string(max_id));
        }
#endif
    }
    
    // Validate pointer is not null
    template<typename T>
    static void validate_ptr(const T* ptr, const std::string& context) {
#if ASM_SAFETY_CHECKS
        if (ptr == nullptr) {
            throw std::invalid_argument("Null pointer in " + context);
        }
#endif
    }
    
    // Validate passport is valid
    static void validate_passport_confidence(float confidence, const std::string& context) {
#if ASM_SAFETY_CHECKS
        if (confidence < 0.0f || confidence > 1.0f) {
            throw std::domain_error(
                "Invalid passport confidence in " + context + 
                ": value=" + std::to_string(confidence));
        }
#endif
    }
};

// Exception types for specific errors
class CorruptedExpertException : public std::runtime_error {
public:
    explicit CorruptedExpertException(const std::string& expert_id)
        : std::runtime_error("Corrupted expert: " + expert_id) {}
};

class SSDReadException : public std::runtime_error {
public:
    explicit SSDReadException(const std::string& message)
        : std::runtime_error("SSD read error: " + message) {}
};

class MemoryPoolOverflowException : public std::runtime_error {
public:
    MemoryPoolOverflowException()
        : std::runtime_error("Memory pool overflow") {}
};

class RouterException : public std::runtime_error {
public:
    explicit RouterException(const std::string& message)
        : std::runtime_error("Router error: " + message) {}
};

} // namespace asm_core
