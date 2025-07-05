/**
 * CPU C-state Control Tool
 * 
 * This tool provides user-space control over CPU idle states (C-states)
 * by manipulating cpuidle sysfs interfaces. C-states determine how deeply
 * CPUs sleep when idle, trading off wake-up latency for power savings.
 * 
 * Key Features:
 * - List available C-states and their properties
 * - Enable/disable specific C-states
 * - Set maximum C-state depth
 * - Monitor C-state residency and transitions
 * - Control idle governor selection
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
#include <numeric>
#include <algorithm>

namespace fs = std::filesystem;

struct CStateInfo {
    std::string name;
    std::string desc;
    unsigned long latency_us;
    unsigned long residency_us;
    unsigned long long usage;
    unsigned long long time_us;
    bool enabled;
};

class CPUCStateControl {
private:
    const std::string cpuidle_base = "/sys/devices/system/cpu/cpu";
    const std::string cpuidle_driver = "/sys/devices/system/cpu/cpuidle";
    int num_cpus;
    
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
    
    int count_cpus() {
        int count = 0;
        for (const auto& entry : fs::directory_iterator("/sys/devices/system/cpu")) {
            if (entry.path().filename().string().find("cpu") == 0) {
                std::string cpu_num = entry.path().filename().string().substr(3);
                if (!cpu_num.empty() && std::all_of(cpu_num.begin(), cpu_num.end(), ::isdigit)) {
                    count++;
                }
            }
        }
        return count;
    }
    
public:
    CPUCStateControl() : num_cpus(count_cpus()) {}
    
    std::vector<CStateInfo> get_cstate_info(int cpu = 0) {
        std::vector<CStateInfo> states;
        std::string base = cpuidle_base + std::to_string(cpu) + "/cpuidle";
        
        if (!fs::exists(base)) {
            throw std::runtime_error("CPU idle interface not available");
        }
        
        for (const auto& entry : fs::directory_iterator(base)) {
            if (entry.path().filename().string().find("state") == 0) {
                CStateInfo info;
                std::string state_path = entry.path().string();
                
                info.name = read_file(state_path + "/name");
                info.desc = read_file(state_path + "/desc");
                
                std::string latency = read_file(state_path + "/latency");
                info.latency_us = latency.empty() ? 0 : std::stoul(latency);
                
                std::string residency = read_file(state_path + "/residency");
                info.residency_us = residency.empty() ? 0 : std::stoul(residency);
                
                std::string usage = read_file(state_path + "/usage");
                info.usage = usage.empty() ? 0 : std::stoull(usage);
                
                std::string time = read_file(state_path + "/time");
                info.time_us = time.empty() ? 0 : std::stoull(time);
                
                std::string disable = read_file(state_path + "/disable");
                info.enabled = (disable == "0");
                
                states.push_back(info);
            }
        }
        
        return states;
    }
    
    void list_cstates(int cpu = 0) {
        auto states = get_cstate_info(cpu);
        
        std::cout << "\nC-states for CPU " << cpu << ":\n";
        std::cout << std::setw(8) << "State" 
                  << std::setw(15) << "Name"
                  << std::setw(35) << "Description"
                  << std::setw(12) << "Latency(us)"
                  << std::setw(12) << "Target(us)"
                  << std::setw(10) << "Enabled\n";
        std::cout << std::string(92, '-') << std::endl;
        
        int idx = 0;
        for (const auto& state : states) {
            std::cout << std::setw(8) << "C" + std::to_string(idx++)
                      << std::setw(15) << state.name
                      << std::setw(35) << state.desc.substr(0, 33)
                      << std::setw(12) << state.latency_us
                      << std::setw(12) << state.residency_us
                      << std::setw(10) << (state.enabled ? "Yes" : "No")
                      << std::endl;
        }
    }
    
    void set_cstate_enabled(int state_idx, bool enable, int cpu = -1) {
        std::vector<int> cpus_to_set;
        if (cpu == -1) {
            for (int i = 0; i < num_cpus; i++) {
                cpus_to_set.push_back(i);
            }
        } else {
            cpus_to_set.push_back(cpu);
        }
        
        for (int c : cpus_to_set) {
            std::string path = cpuidle_base + std::to_string(c) + 
                              "/cpuidle/state" + std::to_string(state_idx) + "/disable";
            write_file(path, enable ? "0" : "1");
        }
        
        std::cout << (enable ? "Enabled" : "Disabled") << " C-state " 
                  << state_idx << " on " 
                  << (cpu == -1 ? "all CPUs" : "CPU " + std::to_string(cpu)) 
                  << std::endl;
    }
    
    void set_max_cstate(int max_state) {
        // Disable all states deeper than max_state
        for (int cpu = 0; cpu < num_cpus; cpu++) {
            auto states = get_cstate_info(cpu);
            for (size_t i = 0; i < states.size(); i++) {
                set_cstate_enabled(i, i <= (size_t)max_state, cpu);
            }
        }
        std::cout << "Set maximum C-state to C" << max_state << " on all CPUs\n";
    }
    
    void list_governors() {
        std::string available = read_file(cpuidle_driver + "/available_governors");
        std::string current = read_file(cpuidle_driver + "/current_governor");
        
        std::cout << "Available idle governors: " << available << std::endl;
        std::cout << "Current idle governor: " << current << std::endl;
    }
    
    void set_governor(const std::string& governor) {
        write_file(cpuidle_driver + "/current_governor", governor);
        std::cout << "Set idle governor to: " << governor << std::endl;
    }
    
    void monitor_cstates(int duration_sec = 10, int sample_interval_ms = 1000) {
        std::cout << "\nMonitoring C-state residency for " << duration_sec << " seconds...\n";
        
        // Get initial state
        std::map<int, std::vector<CStateInfo>> initial_states;
        for (int cpu = 0; cpu < num_cpus; cpu++) {
            initial_states[cpu] = get_cstate_info(cpu);
        }
        
        // Print header
        std::cout << std::setw(10) << "Time(s)";
        for (int cpu = 0; cpu < std::min(4, num_cpus); cpu++) {
            std::cout << std::setw(25) << "CPU" + std::to_string(cpu) + " Distribution(%)";
        }
        std::cout << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() < duration_sec) {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() / 1000.0;
            
            std::cout << std::fixed << std::setprecision(1) << std::setw(10) << elapsed;
            
            // Show distribution for first few CPUs
            for (int cpu = 0; cpu < std::min(4, num_cpus); cpu++) {
                auto current_states = get_cstate_info(cpu);
                
                // Calculate time deltas
                std::string distribution;
                unsigned long long total_delta = 0;
                std::vector<unsigned long long> deltas;
                
                for (size_t i = 0; i < current_states.size(); i++) {
                    unsigned long long delta = current_states[i].time_us - 
                                              initial_states[cpu][i].time_us;
                    deltas.push_back(delta);
                    total_delta += delta;
                }
                
                // Build distribution string
                for (size_t i = 0; i < deltas.size(); i++) {
                    if (total_delta > 0) {
                        int percent = (deltas[i] * 100) / total_delta;
                        if (percent > 0) {
                            distribution += "C" + std::to_string(i) + ":" + 
                                          std::to_string(percent) + "% ";
                        }
                    }
                }
                
                std::cout << std::setw(25) << distribution;
            }
            std::cout << std::endl;
        }
    }
    
    void show_stats(int cpu = 0) {
        auto states = get_cstate_info(cpu);
        
        std::cout << "\nC-state statistics for CPU " << cpu << ":\n";
        std::cout << std::setw(8) << "State"
                  << std::setw(15) << "Name"
                  << std::setw(15) << "Usage Count"
                  << std::setw(20) << "Total Time(ms)"
                  << std::setw(20) << "Avg Residency(us)\n";
        std::cout << std::string(78, '-') << std::endl;
        
        int idx = 0;
        unsigned long long total_time = 0;
        
        for (const auto& state : states) {
            total_time += state.time_us;
        }
        
        for (const auto& state : states) {
            double avg_residency = (state.usage > 0) ? 
                                  (double)state.time_us / state.usage : 0;
            double time_percent = (total_time > 0) ? 
                                 (double)state.time_us * 100 / total_time : 0;
            
            std::cout << std::setw(8) << "C" + std::to_string(idx++)
                      << std::setw(15) << state.name
                      << std::setw(15) << state.usage
                      << std::setw(20) << std::fixed << std::setprecision(1) 
                      << state.time_us / 1000.0
                      << std::setw(20) << std::fixed << std::setprecision(1)
                      << avg_residency
                      << " (" << std::setprecision(1) << time_percent << "%)"
                      << std::endl;
        }
    }
};

void print_usage() {
    std::cout << "CPU C-State Control Tool\n";
    std::cout << "Usage: cpu_cstate_control <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list               List available C-states\n";
    std::cout << "  enable <state>     Enable specific C-state (0-based index)\n";
    std::cout << "  disable <state>    Disable specific C-state\n";
    std::cout << "  max-cstate <n>     Set maximum allowed C-state\n";
    std::cout << "  list-gov           List available idle governors\n";
    std::cout << "  set-gov <name>     Set idle governor (menu|ladder|teo)\n";
    std::cout << "  monitor [seconds]  Monitor C-state residency\n";
    std::cout << "  stats [cpu]        Show C-state statistics\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    try {
        CPUCStateControl ctrl;
        std::string cmd = argv[1];
        
        if (cmd == "list") {
            ctrl.list_cstates();
        } else if (cmd == "enable" && argc >= 3) {
            int state = std::stoi(argv[2]);
            ctrl.set_cstate_enabled(state, true);
        } else if (cmd == "disable" && argc >= 3) {
            int state = std::stoi(argv[2]);
            ctrl.set_cstate_enabled(state, false);
        } else if (cmd == "max-cstate" && argc >= 3) {
            int max_state = std::stoi(argv[2]);
            ctrl.set_max_cstate(max_state);
        } else if (cmd == "list-gov") {
            ctrl.list_governors();
        } else if (cmd == "set-gov" && argc >= 3) {
            ctrl.set_governor(argv[2]);
        } else if (cmd == "monitor") {
            int duration = (argc >= 3) ? std::stoi(argv[2]) : 10;
            ctrl.monitor_cstates(duration);
        } else if (cmd == "stats") {
            int cpu = (argc >= 3) ? std::stoi(argv[2]) : 0;
            ctrl.show_stats(cpu);
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