/**
 * CPU Frequency (P-state/DVFS) Control Tool
 * 
 * This tool provides direct user-space control over CPU frequency scaling
 * by manipulating cpufreq sysfs interfaces. It allows setting frequency
 * governors, min/max frequencies, and specific target frequencies.
 * 
 * Key Features:
 * - List available frequencies and governors
 * - Set frequency scaling governor
 * - Set min/max frequency bounds  
 * - Set specific target frequency (userspace governor)
 * - Monitor current frequency and stats
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
#include <algorithm>
#include <numeric>

namespace fs = std::filesystem;

class CPUFreqControl {
private:
    const std::string cpufreq_base = "/sys/devices/system/cpu/cpufreq";
    std::vector<int> active_cpus;
    
    std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path);
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
    
    std::vector<unsigned long> parse_frequencies(const std::string& freq_str) {
        std::vector<unsigned long> freqs;
        std::istringstream iss(freq_str);
        unsigned long freq;
        while (iss >> freq) {
            freqs.push_back(freq);
        }
        return freqs;
    }

public:
    CPUFreqControl() {
        // Discover active CPU policies
        for (const auto& entry : fs::directory_iterator(cpufreq_base)) {
            if (entry.path().filename().string().find("policy") == 0) {
                std::string policy_num = entry.path().filename().string().substr(6);
                active_cpus.push_back(std::stoi(policy_num));
            }
        }
        std::sort(active_cpus.begin(), active_cpus.end());
    }
    
    void list_governors(int cpu = 0) {
        std::string path = cpufreq_base + "/policy" + std::to_string(cpu) + "/scaling_available_governors";
        std::cout << "Available governors: " << read_file(path) << std::endl;
    }
    
    void list_frequencies(int cpu = 0) {
        std::string path = cpufreq_base + "/policy" + std::to_string(cpu) + "/scaling_available_frequencies";
        std::string freqs_str = read_file(path);
        auto freqs = parse_frequencies(freqs_str);
        
        std::cout << "Available frequencies for CPU " << cpu << ":\n";
        for (auto freq : freqs) {
            std::cout << "  " << freq / 1000 << " MHz (" << freq << " kHz)\n";
        }
    }
    
    void set_governor(const std::string& governor, int cpu = -1) {
        auto cpus_to_set = (cpu == -1) ? active_cpus : std::vector<int>{cpu};
        
        for (int c : cpus_to_set) {
            std::string path = cpufreq_base + "/policy" + std::to_string(c) + "/scaling_governor";
            write_file(path, governor);
            std::cout << "Set CPU " << c << " governor to: " << governor << std::endl;
        }
    }
    
    void set_frequency_limits(unsigned long min_khz, unsigned long max_khz, int cpu = -1) {
        auto cpus_to_set = (cpu == -1) ? active_cpus : std::vector<int>{cpu};
        
        for (int c : cpus_to_set) {
            std::string min_path = cpufreq_base + "/policy" + std::to_string(c) + "/scaling_min_freq";
            std::string max_path = cpufreq_base + "/policy" + std::to_string(c) + "/scaling_max_freq";
            
            // Order matters: increase max before min, decrease min before max
            if (max_khz > std::stoul(read_file(max_path))) {
                write_file(max_path, std::to_string(max_khz));
                write_file(min_path, std::to_string(min_khz));
            } else {
                write_file(min_path, std::to_string(min_khz));
                write_file(max_path, std::to_string(max_khz));
            }
            
            std::cout << "Set CPU " << c << " frequency range: " 
                      << min_khz/1000 << "-" << max_khz/1000 << " MHz\n";
        }
    }
    
    void set_target_frequency(unsigned long freq_khz, int cpu = -1) {
        // First ensure userspace governor is active
        set_governor("userspace", cpu);
        
        auto cpus_to_set = (cpu == -1) ? active_cpus : std::vector<int>{cpu};
        
        for (int c : cpus_to_set) {
            std::string path = cpufreq_base + "/policy" + std::to_string(c) + "/scaling_setspeed";
            write_file(path, std::to_string(freq_khz));
            std::cout << "Set CPU " << c << " frequency to: " << freq_khz/1000 << " MHz\n";
        }
    }
    
    void monitor_frequencies(int duration_sec = 10) {
        std::cout << "Monitoring CPU frequencies for " << duration_sec << " seconds...\n";
        std::cout << std::setw(10) << "Time(s)";
        
        for (int cpu : active_cpus) {
            std::cout << std::setw(12) << "CPU" + std::to_string(cpu) + "(MHz)";
        }
        std::cout << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() < duration_sec) {
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() / 1000.0;
            
            std::cout << std::fixed << std::setprecision(1) << std::setw(10) << elapsed;
            
            for (int cpu : active_cpus) {
                std::string path = cpufreq_base + "/policy" + std::to_string(cpu) + "/scaling_cur_freq";
                try {
                    unsigned long freq = std::stoul(read_file(path));
                    std::cout << std::setw(12) << freq/1000;
                } catch (...) {
                    std::cout << std::setw(12) << "N/A";
                }
            }
            std::cout << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    
    void show_stats(int cpu = 0) {
        std::string stats_path = cpufreq_base + "/policy" + std::to_string(cpu) + "/stats/time_in_state";
        
        std::cout << "\nFrequency residency stats for CPU " << cpu << ":\n";
        std::cout << std::setw(15) << "Frequency(MHz)" << std::setw(15) << "Time(ms)\n";
        
        std::ifstream file(stats_path);
        std::string line;
        while (std::getline(file, line)) {
            unsigned long freq, time;
            std::istringstream iss(line);
            if (iss >> freq >> time) {
                std::cout << std::setw(15) << freq/1000 
                          << std::setw(15) << time/1000 << std::endl;
            }
        }
    }
};

void print_usage() {
    std::cout << "CPU Frequency Control Tool\n";
    std::cout << "Usage: cpu_freq_control <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list-gov           List available governors\n";
    std::cout << "  list-freq          List available frequencies\n";
    std::cout << "  set-gov <name>     Set governor (performance|powersave|ondemand|etc)\n";
    std::cout << "  set-limits <min> <max>  Set frequency limits in MHz\n";
    std::cout << "  set-freq <freq>    Set specific frequency in MHz (userspace governor)\n";
    std::cout << "  monitor [seconds]  Monitor current frequencies\n";
    std::cout << "  stats              Show frequency residency statistics\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    try {
        CPUFreqControl ctrl;
        std::string cmd = argv[1];
        
        if (cmd == "list-gov") {
            ctrl.list_governors();
        } else if (cmd == "list-freq") {
            ctrl.list_frequencies();
        } else if (cmd == "set-gov" && argc >= 3) {
            ctrl.set_governor(argv[2]);
        } else if (cmd == "set-limits" && argc >= 4) {
            unsigned long min_mhz = std::stoul(argv[2]);
            unsigned long max_mhz = std::stoul(argv[3]);
            ctrl.set_frequency_limits(min_mhz * 1000, max_mhz * 1000);
        } else if (cmd == "set-freq" && argc >= 3) {
            unsigned long freq_mhz = std::stoul(argv[2]);
            ctrl.set_target_frequency(freq_mhz * 1000);
        } else if (cmd == "monitor") {
            int duration = (argc >= 3) ? std::stoi(argv[2]) : 10;
            ctrl.monitor_frequencies(duration);
        } else if (cmd == "stats") {
            ctrl.show_stats();
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