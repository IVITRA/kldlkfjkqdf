#pragma once
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace asm_core {

// Power-Aware Thread Pool Scheduler
class PowerAwareScheduler {
public:
    enum class PowerMode {
        PERFORMANCE,    // Max threads, max quality
        BALANCED,       // Medium threads, good quality
        POWER_SAVER     // Min threads, acceptable quality
    };
    
private:
    PowerMode current_mode = PowerMode::BALANCED;
    size_t optimal_thread_count = 4;
    int quantization_bits = 4;
    std::atomic<bool> on_battery{false};
    
public:
    PowerAwareScheduler() {
        // Detect initial power state
        update_power_state();
        set_mode(PowerMode::BALANCED);
    }
    
    // Set power mode manually
    void set_mode(PowerMode mode) {
        current_mode = mode;
        
        switch(mode) {
            case PowerMode::POWER_SAVER:
                optimal_thread_count = 2;
                quantization_bits = 2; // Lower quality, less computation
                std::cout << "⚡ Power Saver mode: 2 threads, 2-bit quant" << std::endl;
                break;
                
            case PowerMode::BALANCED:
                optimal_thread_count = 4;
                quantization_bits = 4;
                std::cout << "⚖️  Balanced mode: 4 threads, 4-bit quant" << std::endl;
                break;
                
            case PowerMode::PERFORMANCE:
                optimal_thread_count = std::thread::hardware_concurrency();
                quantization_bits = 4; // Keep 4-bit for quality
                std::cout << "🚀 Performance mode: " << optimal_thread_count << " threads" << std::endl;
                break;
        }
    }
    
    // Auto-detect battery state and adjust
    void update_power_state() {
        bool was_on_battery = on_battery.load();
        on_battery.store(is_on_battery());
        
        // Auto-switch to power saver if on battery
        if (on_battery.load() && !was_on_battery) {
            std::cout << "\n🔋 Battery detected - switching to Power Saver mode\n" << std::endl;
            set_mode(PowerMode::POWER_SAVER);
        } else if (!on_battery.load() && was_on_battery) {
            std::cout << "\n🔌 AC power detected - switching to Balanced mode\n" << std::endl;
            set_mode(PowerMode::BALANCED);
        }
    }
    
    // Get optimal thread count for current mode
    size_t get_thread_count() const {
        return optimal_thread_count;
    }
    
    // Get quantization bits for current mode
    int get_quantization_bits() const {
        return quantization_bits;
    }
    
    // Get current power mode
    PowerMode get_mode() const {
        return current_mode;
    }
    
    // Check if running on battery
    bool is_on_battery() const {
        return on_battery.load();
    }
    
    // Get mode name
    static const char* mode_name(PowerMode mode) {
        switch(mode) {
            case PowerMode::PERFORMANCE: return "Performance";
            case PowerMode::BALANCED: return "Balanced";
            case PowerMode::POWER_SAVER: return "Power Saver";
            default: return "Unknown";
        }
    }
    
    // Print current status
    void print_status() const {
        std::cout << "\n=== Power Management ===" << std::endl;
        std::cout << "Mode: " << mode_name(current_mode) << std::endl;
        std::cout << "Thread Count: " << optimal_thread_count << std::endl;
        std::cout << "Quantization: " << quantization_bits << "-bit" << std::endl;
        std::cout << "On Battery: " << (on_battery.load() ? "Yes" : "No") << std::endl;
        std::cout << "========================\n" << std::endl;
    }

private:
    // Platform-specific battery detection
    bool is_on_battery() {
#ifdef _WIN32
        SYSTEM_POWER_STATUS powerStatus;
        if (GetSystemPowerStatus(&powerStatus)) {
            return powerStatus.ACLineStatus == 0;
        }
#elif __linux__
        // Check /sys/class/power_supply/BAT0/status
        FILE* fp = fopen("/sys/class/power_supply/BAT0/status", "r");
        if (fp) {
            char status[32];
            if (fgets(status, sizeof(status), fp)) {
                fclose(fp);
                return (std::string(status).find("Discharging") != std::string::npos);
            }
            fclose(fp);
        }
#elif __APPLE__
        // macOS battery detection would use IOKit
        // Simplified: assume AC power
        return false;
#endif
        return false; // Default to AC power
    }
};

} // namespace asm_core
