/**
 * Thermal Cap Control Tool
 * 
 * This tool provides user-space control over thermal throttling by
 * manipulating cooling devices and thermal zones via sysfs. It allows
 * proactive thermal management to prevent hard throttling.
 * 
 * Key Features:
 * - Monitor thermal zones and temperatures
 * - Control cooling device states
 * - Set temperature-based frequency caps
 * - Implement custom thermal policies
 * - Track thermal throttling events
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>
#include <map>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace fs = std::filesystem;

struct ThermalZone {
    int id;
    std::string type;
    int temp_mC;  // millicelsius
    std::vector<int> trip_points;
    std::vector<std::string> trip_types;
};

struct CoolingDevice {
    int id;
    std::string type;
    unsigned long cur_state;
    unsigned long max_state;
};

class ThermalCapControl {
private:
    const std::string thermal_base = "/sys/class/thermal";
    const std::string cpufreq_base = "/sys/devices/system/cpu/cpufreq";
    std::atomic<bool> monitor_active{false};
    std::mutex policy_mutex;
    
    // Thermal policy parameters
    struct ThermalPolicy {
        int temp_low_mC = 70000;   // 70°C - start gentle throttling
        int temp_high_mC = 85000;  // 85°C - aggressive throttling
        int temp_critical_mC = 95000; // 95°C - maximum throttling
        unsigned long freq_min_khz = 800000;  // Minimum frequency
        unsigned long freq_max_khz = 0;  // Will be set to CPU max
        bool enabled = false;
    } policy;
    
    std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        std::string content;
        std::getline(file, content);
        return content;
    }
    
    void write_file(const std::string& path, const std::string& value) {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path);
        }
        file << value;
        if (!file.good()) {
            throw std::runtime_error("Failed to write to: " + path);
        }
    }
    
    unsigned long get_cpu_max_freq() {
        std::string path = cpufreq_base + "/policy0/cpuinfo_max_freq";
        std::string freq_str = read_file(path);
        return freq_str.empty() ? 3600000 : std::stoul(freq_str);
    }
    
public:
    ThermalCapControl() {
        policy.freq_max_khz = get_cpu_max_freq();
    }
    
    std::vector<ThermalZone> get_thermal_zones() {
        std::vector<ThermalZone> zones;
        
        for (const auto& entry : fs::directory_iterator(thermal_base)) {
            if (entry.path().filename().string().find("thermal_zone") == 0) {
                ThermalZone zone;
                std::string zone_path = entry.path().string();
                
                // Extract zone ID
                std::string zone_name = entry.path().filename().string();
                zone.id = std::stoi(zone_name.substr(12));
                
                // Read zone type
                zone.type = read_file(zone_path + "/type");
                
                // Read temperature
                std::string temp_str = read_file(zone_path + "/temp");
                zone.temp_mC = temp_str.empty() ? 0 : std::stoi(temp_str);
                
                // Read trip points
                for (int i = 0; i < 10; i++) {
                    std::string trip_temp = read_file(zone_path + "/trip_point_" + 
                                                    std::to_string(i) + "_temp");
                    std::string trip_type = read_file(zone_path + "/trip_point_" + 
                                                    std::to_string(i) + "_type");
                    
                    if (trip_temp.empty()) break;
                    
                    zone.trip_points.push_back(std::stoi(trip_temp));
                    zone.trip_types.push_back(trip_type);
                }
                
                zones.push_back(zone);
            }
        }
        
        return zones;
    }
    
    std::vector<CoolingDevice> get_cooling_devices() {
        std::vector<CoolingDevice> devices;
        
        for (const auto& entry : fs::directory_iterator(thermal_base)) {
            if (entry.path().filename().string().find("cooling_device") == 0) {
                CoolingDevice device;
                std::string device_path = entry.path().string();
                
                // Extract device ID
                std::string device_name = entry.path().filename().string();
                device.id = std::stoi(device_name.substr(14));
                
                // Read device info
                device.type = read_file(device_path + "/type");
                
                std::string cur_state = read_file(device_path + "/cur_state");
                device.cur_state = cur_state.empty() ? 0 : std::stoul(cur_state);
                
                std::string max_state = read_file(device_path + "/max_state");
                device.max_state = max_state.empty() ? 0 : std::stoul(max_state);
                
                devices.push_back(device);
            }
        }
        
        return devices;
    }
    
    void list_thermal_info() {
        auto zones = get_thermal_zones();
        auto devices = get_cooling_devices();
        
        std::cout << "\nThermal Zones:\n";
        std::cout << std::setw(6) << "ID" 
                  << std::setw(20) << "Type"
                  << std::setw(12) << "Temp(°C)"
                  << std::setw(40) << "Trip Points\n";
        std::cout << std::string(78, '-') << std::endl;
        
        for (const auto& zone : zones) {
            std::cout << std::setw(6) << zone.id
                      << std::setw(20) << zone.type
                      << std::setw(12) << std::fixed << std::setprecision(1) 
                      << zone.temp_mC / 1000.0;
            
            std::string trips;
            for (size_t i = 0; i < zone.trip_points.size(); i++) {
                trips += zone.trip_types[i] + ":" + 
                        std::to_string(zone.trip_points[i] / 1000) + "°C ";
            }
            std::cout << std::setw(40) << trips << std::endl;
        }
        
        std::cout << "\nCooling Devices:\n";
        std::cout << std::setw(6) << "ID"
                  << std::setw(20) << "Type"
                  << std::setw(12) << "Current"
                  << std::setw(12) << "Max\n";
        std::cout << std::string(50, '-') << std::endl;
        
        for (const auto& device : devices) {
            std::cout << std::setw(6) << device.id
                      << std::setw(20) << device.type
                      << std::setw(12) << device.cur_state
                      << std::setw(12) << device.max_state
                      << std::endl;
        }
    }
    
    void set_cooling_device_state(int device_id, unsigned long state) {
        std::string path = thermal_base + "/cooling_device" + 
                          std::to_string(device_id) + "/cur_state";
        write_file(path, std::to_string(state));
        std::cout << "Set cooling device " << device_id << " to state " << state << std::endl;
    }
    
    void set_cpu_frequency_cap(unsigned long freq_khz) {
        // Set frequency cap on all CPU policies
        for (const auto& entry : fs::directory_iterator(cpufreq_base)) {
            if (entry.path().filename().string().find("policy") == 0) {
                std::string max_path = entry.path().string() + "/scaling_max_freq";
                write_file(max_path, std::to_string(freq_khz));
            }
        }
        std::cout << "Set CPU frequency cap to " << freq_khz / 1000 << " MHz\n";
    }
    
    void configure_thermal_policy(int temp_low_C, int temp_high_C, int temp_critical_C) {
        std::lock_guard<std::mutex> lock(policy_mutex);
        policy.temp_low_mC = temp_low_C * 1000;
        policy.temp_high_mC = temp_high_C * 1000;
        policy.temp_critical_mC = temp_critical_C * 1000;
        policy.enabled = true;
        
        std::cout << "Configured thermal policy:\n";
        std::cout << "  Low threshold: " << temp_low_C << "°C\n";
        std::cout << "  High threshold: " << temp_high_C << "°C\n";
        std::cout << "  Critical threshold: " << temp_critical_C << "°C\n";
    }
    
    void apply_thermal_policy() {
        auto zones = get_thermal_zones();
        
        // Find CPU thermal zone
        int cpu_temp_mC = 0;
        for (const auto& zone : zones) {
            if (zone.type.find("cpu") != std::string::npos || 
                zone.type.find("x86_pkg_temp") != std::string::npos) {
                cpu_temp_mC = zone.temp_mC;
                break;
            }
        }
        
        if (cpu_temp_mC == 0) {
            // Fallback: use highest temperature
            for (const auto& zone : zones) {
                cpu_temp_mC = std::max(cpu_temp_mC, zone.temp_mC);
            }
        }
        
        std::lock_guard<std::mutex> lock(policy_mutex);
        if (!policy.enabled) return;
        
        // Calculate frequency cap based on temperature
        unsigned long freq_cap_khz = policy.freq_max_khz;
        
        if (cpu_temp_mC >= policy.temp_critical_mC) {
            // Critical: minimum frequency
            freq_cap_khz = policy.freq_min_khz;
        } else if (cpu_temp_mC >= policy.temp_high_mC) {
            // High: linear interpolation between min and 50% of max
            double ratio = (double)(policy.temp_critical_mC - cpu_temp_mC) / 
                          (policy.temp_critical_mC - policy.temp_high_mC);
            freq_cap_khz = policy.freq_min_khz + 
                          (policy.freq_max_khz * 0.5 - policy.freq_min_khz) * ratio;
        } else if (cpu_temp_mC >= policy.temp_low_mC) {
            // Low: linear interpolation between 50% and 100% of max
            double ratio = (double)(policy.temp_high_mC - cpu_temp_mC) / 
                          (policy.temp_high_mC - policy.temp_low_mC);
            freq_cap_khz = policy.freq_max_khz * 0.5 + 
                          (policy.freq_max_khz * 0.5) * ratio;
        }
        
        set_cpu_frequency_cap(freq_cap_khz);
    }
    
    void monitor_and_cap(int interval_ms = 1000) {
        monitor_active = true;
        
        std::cout << "\nMonitoring temperature and applying thermal caps...\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        
        std::cout << std::setw(12) << "Time(s)"
                  << std::setw(12) << "CPU Temp"
                  << std::setw(15) << "Freq Cap(MHz)"
                  << std::setw(20) << "Policy State\n";
        std::cout << std::string(59, '-') << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        
        while (monitor_active) {
            auto zones = get_thermal_zones();
            
            // Find CPU temperature
            int cpu_temp_mC = 0;
            for (const auto& zone : zones) {
                if (zone.type.find("cpu") != std::string::npos || 
                    zone.type.find("x86_pkg_temp") != std::string::npos) {
                    cpu_temp_mC = zone.temp_mC;
                    break;
                }
            }
            
            // Apply policy and get current cap
            apply_thermal_policy();
            
            // Read current frequency cap
            std::string cap_str = read_file(cpufreq_base + "/policy0/scaling_max_freq");
            unsigned long current_cap = cap_str.empty() ? 0 : std::stoul(cap_str);
            
            // Determine policy state
            std::string state;
            {
                std::lock_guard<std::mutex> lock(policy_mutex);
                if (!policy.enabled) {
                    state = "Disabled";
                } else if (cpu_temp_mC >= policy.temp_critical_mC) {
                    state = "CRITICAL";
                } else if (cpu_temp_mC >= policy.temp_high_mC) {
                    state = "High throttle";
                } else if (cpu_temp_mC >= policy.temp_low_mC) {
                    state = "Low throttle";
                } else {
                    state = "Normal";
                }
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() / 1000.0;
            
            std::cout << std::fixed << std::setprecision(1)
                      << std::setw(12) << elapsed
                      << std::setw(12) << cpu_temp_mC / 1000.0
                      << std::setw(15) << current_cap / 1000
                      << std::setw(20) << state
                      << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
    
    void disable_policy() {
        std::lock_guard<std::mutex> lock(policy_mutex);
        policy.enabled = false;
        
        // Restore max frequency
        set_cpu_frequency_cap(policy.freq_max_khz);
        
        std::cout << "Thermal policy disabled, frequency cap removed\n";
    }
};

void print_usage() {
    std::cout << "Thermal Cap Control Tool\n";
    std::cout << "Usage: thermal_cap_control <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list                          List thermal zones and cooling devices\n";
    std::cout << "  set-cooling <id> <state>      Set cooling device state\n";
    std::cout << "  set-cap <freq_mhz>           Set CPU frequency cap\n";
    std::cout << "  policy <low> <high> <crit>   Configure thermal policy (temps in °C)\n";
    std::cout << "  monitor [interval_ms]         Monitor and apply thermal caps\n";
    std::cout << "  disable                       Disable thermal policy\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    try {
        ThermalCapControl ctrl;
        std::string cmd = argv[1];
        
        if (cmd == "list") {
            ctrl.list_thermal_info();
        } else if (cmd == "set-cooling" && argc >= 4) {
            int device_id = std::stoi(argv[2]);
            unsigned long state = std::stoul(argv[3]);
            ctrl.set_cooling_device_state(device_id, state);
        } else if (cmd == "set-cap" && argc >= 3) {
            unsigned long freq_mhz = std::stoul(argv[2]);
            ctrl.set_cpu_frequency_cap(freq_mhz * 1000);
        } else if (cmd == "policy" && argc >= 5) {
            int temp_low = std::stoi(argv[2]);
            int temp_high = std::stoi(argv[3]);
            int temp_crit = std::stoi(argv[4]);
            ctrl.configure_thermal_policy(temp_low, temp_high, temp_crit);
        } else if (cmd == "monitor") {
            int interval = (argc >= 3) ? std::stoi(argv[2]) : 1000;
            ctrl.monitor_and_cap(interval);
        } else if (cmd == "disable") {
            ctrl.disable_policy();
        } else {
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Note: This tool requires root privileges\n";
        return 1;
    }
    
    return 0;
}