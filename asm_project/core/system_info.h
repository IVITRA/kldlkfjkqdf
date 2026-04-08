#pragma once
#include <cstdint>
#include <string>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#elif __linux__
#include <sys/sysinfo.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace asm_core {

class SystemInfo {
public:
    // Get total system RAM in bytes
    static uint64_t get_total_ram() {
#ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status)) {
            return status.ullTotalPhys;
        }
#elif __linux__
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            return info.totalram * info.mem_unit;
        }
#elif __APPLE__
        int64_t memory = 0;
        size_t length = sizeof(memory);
        if (sysctlbyname("hw.memsize", &memory, &length, nullptr, 0) == 0) {
            return memory;
        }
#endif
        return 0; // Unknown
    }
    
    // Get available RAM in bytes
    static uint64_t get_available_ram() {
#ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status)) {
            return status.ullAvailPhys;
        }
#elif __linux__
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            return info.freeram * info.mem_unit;
        }
#elif __APPLE__
        // macOS doesn't have direct available memory API
        return get_total_ram() / 2; // Rough estimate
#endif
        return 0;
    }
    
    // Check if system meets minimum requirements
    static bool meets_minimum_requirements(uint64_t min_ram = 4ULL * 1024 * 1024 * 1024) {
        uint64_t total = get_total_ram();
        return total >= min_ram;
    }
    
    // Print system info
    static void print_info() {
        uint64_t total = get_total_ram();
        uint64_t available = get_available_ram();
        
        std::cout << "\n=== System Information ===" << std::endl;
        std::cout << "Total RAM: " << format_bytes(total) << std::endl;
        std::cout << "Available RAM: " << format_bytes(available) << std::endl;
        
        if (total < 4ULL * 1024 * 1024 * 1024) {
            std::cout << "⚠️  WARNING: RAM < 4GB. Enabling ultra-compact mode." << std::endl;
        } else if (total < 8ULL * 1024 * 1024 * 1024) {
            std::cout << "✓ System meets minimum requirements (4-8GB)" << std::endl;
        } else {
            std::cout << "✓ System has ample RAM (8GB+)" << std::endl;
        }
        std::cout << "========================\n" << std::endl;
    }
    
private:
    static std::string format_bytes(uint64_t bytes) {
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

} // namespace asm_core
