/**
 * GPU DevFreq Control Tool
 * 
 * This tool provides user-space control over GPU frequency scaling
 * via the devfreq framework. It supports both integrated and discrete
 * GPUs, allowing dynamic frequency management for power efficiency.
 * 
 * Key Features:
 * - List available GPU devices and frequencies
 * - Set frequency governors and policies
 * - Monitor GPU frequency and utilization
 * - Coordinate GPU frequency with CPU workloads
 * - Support for multiple GPU vendors (Intel, AMD, NVIDIA via nouveau)
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
#include <regex>

namespace fs = std::filesystem;

struct GPUDevice {
    std::string name;
    std::string path;
    std::string driver;
    unsigned long cur_freq;
    unsigned long min_freq;
    unsigned long max_freq;
    std::vector<unsigned long> available_freqs;
    std::string governor;
    std::vector<std::string> available_governors;
};

class GPUDevfreqControl {
private:
    const std::string devfreq_base = "/sys/class/devfreq";
    const std::string drm_base = "/sys/class/drm";
    std::vector<GPUDevice> gpu_devices;
    
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
    
    std::string get_gpu_name_from_path(const std::string& devfreq_path) {
        // Try to extract meaningful GPU name from devfreq path
        std::string name = fs::path(devfreq_path).filename().string();
        
        // Check if it's a PCI device
        if (name.find(".gpu") != std::string::npos) {
            // Intel integrated GPU
            return "Intel Integrated GPU";
        } else if (name.find("amdgpu") != std::string::npos) {
            // AMD GPU
            return "AMD GPU";
        } else if (name.find("nouveau") != std::string::npos) {
            // NVIDIA GPU (nouveau driver)
            return "NVIDIA GPU (nouveau)";
        }
        
        // Try to get more info from DRM
        for (const auto& drm_entry : fs::directory_iterator(drm_base)) {
            std::string drm_name = drm_entry.path().filename().string();
            if (drm_name.find("card") == 0) {
                std::string drm_dev = drm_entry.path().string() + "/device";
                if (fs::exists(drm_dev)) {
                    // Check if this DRM device matches our devfreq device
                    std::string drm_real = fs::canonical(drm_dev).string();
                    std::string dev_real = fs::canonical(devfreq_path).string();
                    
                    if (drm_real.find(dev_real) != std::string::npos ||
                        dev_real.find(drm_real) != std::string::npos) {
                        return "GPU " + drm_name;
                    }
                }
            }
        }
        
        return name;
    }
    
    std::vector<unsigned long> parse_available_frequencies(const std::string& freq_str) {
        std::vector<unsigned long> freqs;
        std::istringstream iss(freq_str);
        unsigned long freq;
        while (iss >> freq) {
            freqs.push_back(freq);
        }
        std::sort(freqs.begin(), freqs.end());
        return freqs;
    }
    
public:
    GPUDevfreqControl() {
        discover_gpu_devices();
    }
    
    void discover_gpu_devices() {
        gpu_devices.clear();
        
        if (!fs::exists(devfreq_base)) {
            std::cout << "DevFreq not available on this system\n";
            return;
        }
        
        for (const auto& entry : fs::directory_iterator(devfreq_base)) {
            GPUDevice device;
            device.path = entry.path().string();
            
            // Skip non-GPU devices
            std::string name = entry.path().filename().string();
            if (name.find("gpu") == std::string::npos && 
                name.find("amdgpu") == std::string::npos &&
                name.find("nouveau") == std::string::npos &&
                !fs::exists(device.path + "/device/drm")) {
                continue;
            }
            
            device.name = get_gpu_name_from_path(device.path);
            
            // Read current frequency
            std::string cur_freq_str = read_file(device.path + "/cur_freq");
            device.cur_freq = cur_freq_str.empty() ? 0 : std::stoul(cur_freq_str);
            
            // Read min/max frequencies
            std::string min_freq_str = read_file(device.path + "/min_freq");
            device.min_freq = min_freq_str.empty() ? 0 : std::stoul(min_freq_str);
            
            std::string max_freq_str = read_file(device.path + "/max_freq");
            device.max_freq = max_freq_str.empty() ? 0 : std::stoul(max_freq_str);
            
            // Read available frequencies
            std::string avail_freqs = read_file(device.path + "/available_frequencies");
            if (!avail_freqs.empty()) {
                device.available_freqs = parse_available_frequencies(avail_freqs);
            }
            
            // Read governor info
            device.governor = read_file(device.path + "/governor");
            
            std::string avail_govs = read_file(device.path + "/available_governors");
            if (!avail_govs.empty()) {
                std::istringstream iss(avail_govs);
                std::string gov;
                while (iss >> gov) {
                    device.available_governors.push_back(gov);
                }
            }
            
            gpu_devices.push_back(device);
        }
    }
    
    void list_devices() {
        if (gpu_devices.empty()) {
            std::cout << "No GPU devices with DevFreq support found\n";
            return;
        }
        
        std::cout << "\nGPU DevFreq Devices:\n";
        std::cout << std::string(80, '=') << "\n";
        
        for (size_t i = 0; i < gpu_devices.size(); i++) {
            const auto& dev = gpu_devices[i];
            std::cout << "\nDevice " << i << ": " << dev.name << "\n";
            std::cout << "  Path: " << dev.path << "\n";
            std::cout << "  Current frequency: " << dev.cur_freq / 1000000 << " MHz\n";
            std::cout << "  Frequency range: " << dev.min_freq / 1000000 << " - " 
                      << dev.max_freq / 1000000 << " MHz\n";
            
            if (!dev.available_freqs.empty()) {
                std::cout << "  Available frequencies: ";
                for (auto freq : dev.available_freqs) {
                    std::cout << freq / 1000000 << " ";
                }
                std::cout << "MHz\n";
            }
            
            std::cout << "  Current governor: " << dev.governor << "\n";
            if (!dev.available_governors.empty()) {
                std::cout << "  Available governors: ";
                for (const auto& gov : dev.available_governors) {
                    std::cout << gov << " ";
                }
                std::cout << "\n";
            }
        }
    }
    
    void set_governor(int device_idx, const std::string& governor) {
        if (device_idx >= gpu_devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        auto& device = gpu_devices[device_idx];
        write_file(device.path + "/governor", governor);
        device.governor = governor;
        
        std::cout << "Set " << device.name << " governor to: " << governor << "\n";
    }
    
    void set_frequency_range(int device_idx, unsigned long min_mhz, unsigned long max_mhz) {
        if (device_idx >= gpu_devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        auto& device = gpu_devices[device_idx];
        unsigned long min_hz = min_mhz * 1000000;
        unsigned long max_hz = max_mhz * 1000000;
        
        // Clamp to device limits
        min_hz = std::max(min_hz, device.min_freq);
        max_hz = std::min(max_hz, device.max_freq);
        
        // Order matters when setting limits
        if (max_hz > device.max_freq) {
            write_file(device.path + "/max_freq", std::to_string(max_hz));
            write_file(device.path + "/min_freq", std::to_string(min_hz));
        } else {
            write_file(device.path + "/min_freq", std::to_string(min_hz));
            write_file(device.path + "/max_freq", std::to_string(max_hz));
        }
        
        device.min_freq = min_hz;
        device.max_freq = max_hz;
        
        std::cout << "Set " << device.name << " frequency range: " 
                  << min_mhz << "-" << max_mhz << " MHz\n";
    }
    
    void monitor_frequencies(int duration_sec = 30, int interval_ms = 500) {
        if (gpu_devices.empty()) {
            std::cout << "No GPU devices to monitor\n";
            return;
        }
        
        std::cout << "\nMonitoring GPU frequencies for " << duration_sec << " seconds...\n\n";
        
        // Print header
        std::cout << std::setw(10) << "Time(s)";
        for (size_t i = 0; i < gpu_devices.size(); i++) {
            std::cout << std::setw(20) << gpu_devices[i].name + "(MHz)";
        }
        std::cout << "\n" << std::string(10 + 20 * gpu_devices.size(), '-') << "\n";
        
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() < duration_sec) {
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() / 1000.0;
            
            std::cout << std::fixed << std::setprecision(1) << std::setw(10) << elapsed;
            
            for (auto& device : gpu_devices) {
                std::string cur_freq_str = read_file(device.path + "/cur_freq");
                unsigned long cur_freq = cur_freq_str.empty() ? 0 : std::stoul(cur_freq_str);
                device.cur_freq = cur_freq;
                
                std::cout << std::setw(20) << cur_freq / 1000000;
            }
            std::cout << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
    
    void show_gpu_stats(int device_idx) {
        if (device_idx >= gpu_devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        auto& device = gpu_devices[device_idx];
        
        std::cout << "\nGPU Statistics for " << device.name << ":\n";
        std::cout << std::string(50, '-') << "\n";
        
        // Try to read utilization if available
        std::string load_path = device.path + "/gpu_load";
        std::string load = read_file(load_path);
        if (!load.empty()) {
            std::cout << "GPU Load: " << load << "%\n";
        }
        
        // Read trans_stat for frequency transition statistics
        std::string trans_stat_path = device.path + "/trans_stat";
        std::ifstream trans_file(trans_stat_path);
        if (trans_file.is_open()) {
            std::cout << "\nFrequency Transition Statistics:\n";
            std::cout << "From/To (MHz)  ";
            
            std::string line;
            bool first_line = true;
            while (std::getline(trans_file, line)) {
                if (first_line) {
                    // Header line
                    std::cout << line << "\n";
                    first_line = false;
                } else if (line.find("Total transition") == std::string::npos) {
                    // Frequency transition data
                    std::istringstream iss(line);
                    unsigned long from_freq;
                    iss >> from_freq;
                    std::cout << std::setw(8) << from_freq / 1000000 << "      ";
                    
                    unsigned long count;
                    while (iss >> count) {
                        std::cout << std::setw(8) << count << " ";
                    }
                    std::cout << "\n";
                }
            }
        }
        
        // Try vendor-specific stats
        if (device.name.find("Intel") != std::string::npos) {
            // Intel GPU specific stats
            std::string rc6_path = "/sys/class/drm/card0/power/rc6_residency_ms";
            std::string rc6_time = read_file(rc6_path);
            if (!rc6_time.empty()) {
                std::cout << "\nIntel GPU RC6 residency: " << rc6_time << " ms\n";
            }
        }
    }
    
    void set_performance_mode(int device_idx) {
        if (device_idx >= gpu_devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        auto& device = gpu_devices[device_idx];
        
        // Set performance governor if available
        if (std::find(device.available_governors.begin(), 
                     device.available_governors.end(), 
                     "performance") != device.available_governors.end()) {
            set_governor(device_idx, "performance");
        }
        
        // Set to maximum frequency
        set_frequency_range(device_idx, 
                           device.max_freq / 1000000, 
                           device.max_freq / 1000000);
        
        std::cout << device.name << " set to performance mode\n";
    }
    
    void set_powersave_mode(int device_idx) {
        if (device_idx >= gpu_devices.size()) {
            throw std::runtime_error("Invalid device index");
        }
        
        auto& device = gpu_devices[device_idx];
        
        // Set powersave governor if available
        if (std::find(device.available_governors.begin(), 
                     device.available_governors.end(), 
                     "powersave") != device.available_governors.end()) {
            set_governor(device_idx, "powersave");
        }
        
        // Allow full frequency range for dynamic scaling
        unsigned long min_safe = device.available_freqs.empty() ? 
                                device.min_freq : device.available_freqs.front();
        
        set_frequency_range(device_idx, 
                           min_safe / 1000000, 
                           device.max_freq / 1000000);
        
        std::cout << device.name << " set to powersave mode\n";
    }
};

void print_usage() {
    std::cout << "GPU DevFreq Control Tool\n";
    std::cout << "Usage: gpu_devfreq_control <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list                              List GPU devices\n";
    std::cout << "  set-gov <device> <governor>       Set governor\n";
    std::cout << "  set-freq <device> <min> <max>     Set frequency range (MHz)\n";
    std::cout << "  performance <device>              Set to performance mode\n";
    std::cout << "  powersave <device>                Set to powersave mode\n";
    std::cout << "  monitor [seconds]                 Monitor GPU frequencies\n";
    std::cout << "  stats <device>                    Show GPU statistics\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    try {
        GPUDevfreqControl ctrl;
        std::string cmd = argv[1];
        
        if (cmd == "list") {
            ctrl.list_devices();
        } else if (cmd == "set-gov" && argc >= 4) {
            int device = std::stoi(argv[2]);
            ctrl.set_governor(device, argv[3]);
        } else if (cmd == "set-freq" && argc >= 5) {
            int device = std::stoi(argv[2]);
            unsigned long min_mhz = std::stoul(argv[3]);
            unsigned long max_mhz = std::stoul(argv[4]);
            ctrl.set_frequency_range(device, min_mhz, max_mhz);
        } else if (cmd == "performance" && argc >= 3) {
            int device = std::stoi(argv[2]);
            ctrl.set_performance_mode(device);
        } else if (cmd == "powersave" && argc >= 3) {
            int device = std::stoi(argv[2]);
            ctrl.set_powersave_mode(device);
        } else if (cmd == "monitor") {
            int duration = (argc >= 3) ? std::stoi(argv[2]) : 30;
            ctrl.monitor_frequencies(duration);
        } else if (cmd == "stats" && argc >= 3) {
            int device = std::stoi(argv[2]);
            ctrl.show_gpu_stats(device);
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