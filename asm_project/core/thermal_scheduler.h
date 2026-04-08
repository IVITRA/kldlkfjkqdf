#pragma once
#include <atomic>
#include <string>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace asm_core {

// Thermal-Aware Scheduler to prevent CPU throttling
class ThermalAwareScheduler {
private:
    std::atomic<int> max_parallel_experts{10};
    std::atomic<float> current_temp{0.0f};
    std::atomic<bool> thermal_throttling{false};
    
    static constexpr float CRITICAL_TEMP = 85.0f; // °C
    static constexpr float WARNING_TEMP = 75.0f;  // °C
    static constexpr float NORMAL_TEMP = 65.0f;   // °C
    
public:
    // Get CPU temperature (platform-specific)
    float get_cpu_temp() {
#ifdef __linux__
        // Linux: read from thermal zone
        FILE* fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (fp) {
            int temp_raw;
            if (fscanf(fp, "%d", &temp_raw) == 1) {
                fclose(fp);
                return temp_raw / 1000.0f; // Convert millidegrees to degrees
            }
            fclose(fp);
        }
#elif defined(_WIN32)
        // Windows: WMI query (simplified - returns 0 if unavailable)
        // In production, use OpenHardwareMonitor or similar
        return 0.0f;
#endif
        return 0.0f; // Temperature unavailable
    }
    
    // Monitor and adjust based on temperature
    void throttle_if_needed() {
        current_temp.store(get_cpu_temp());
        float temp = current_temp.load();
        
        if (temp <= 0.0f) {
            // Temperature monitoring unavailable, skip
            return;
        }
        
        if (temp > CRITICAL_TEMP) {
            // Critical: Aggressive throttling
            max_parallel_experts.store(2);
            thermal_throttling.store(true);
            std::cerr << "\n🌡️  CRITICAL: CPU at " << temp 
                      << "°C - Throttling to 2 experts\n" << std::endl;
            
        } else if (temp > WARNING_TEMP) {
            // Warning: Moderate throttling
            max_parallel_experts.store(4);
            thermal_throttling.store(true);
            std::cout << "\n⚠️  WARNING: CPU at " << temp 
                      << "°C - Reduced to 4 experts\n" << std::endl;
            
        } else if (temp < NORMAL_TEMP && thermal_throttling.load()) {
            // Recovered: Back to normal
            max_parallel_experts.store(10);
            thermal_throttling.store(false);
            std::cout << "\n✅ Temperature normalized at " << temp 
                      << "°C - Restored to 10 experts\n" << std::endl;
        }
    }
    
    // Get current limit
    int get_max_parallel_experts() const {
        return max_parallel_experts.load();
    }
    
    // Check if throttling
    bool is_throttling() const {
        return thermal_throttling.load();
    }
    
    // Get current temperature
    float get_temperature() const {
        return current_temp.load();
    }
    
    // Print thermal status
    void print_status() const {
        float temp = current_temp.load();
        std::cout << "\n=== Thermal Status ===" << std::endl;
        if (temp > 0.0f) {
            std::cout << "CPU Temp: " << temp << "°C" << std::endl;
        } else {
            std::cout << "CPU Temp: N/A (monitoring unavailable)" << std::endl;
        }
        std::cout << "Max Parallel Experts: " << max_parallel_experts.load() << std::endl;
        std::cout << "Throttling: " << (thermal_throttling.load() ? "Yes" : "No") << std::endl;
        std::cout << "=====================\n" << std::endl;
    }
    
    // Set CPU governor (Linux only)
    void set_cpu_governor(const std::string& governor) {
#ifdef __linux__
        std::string cmd = "echo " + governor + " | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor";
        system(cmd.c_str());
        std::cout << "CPU governor set to: " << governor << std::endl;
#endif
    }
};

} // namespace asm_core
