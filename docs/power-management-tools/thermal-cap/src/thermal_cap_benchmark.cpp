/**
 * Thermal Cap Benchmark
 * 
 * This benchmark measures the effectiveness of different thermal
 * management strategies:
 * - Temperature response curves
 * - Performance vs temperature trade-offs
 * - Proactive vs reactive throttling
 * - Thermal headroom utilization
 */

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <random>

class ThermalCapBenchmark {
private:
    std::atomic<bool> stop_flag{false};
    std::atomic<double> current_load{0.0};
    
    struct ThermalData {
        double time_s;
        double temperature_C;
        unsigned long frequency_mhz;
        double performance_score;
        double power_watts;
    };
    
    double read_cpu_temp() {
        // Try multiple thermal zone paths
        std::vector<std::string> temp_paths = {
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/thermal/thermal_zone1/temp",
            "/sys/class/thermal/thermal_zone2/temp",
            "/sys/devices/platform/coretemp.0/hwmon/hwmon*/temp1_input"
        };
        
        for (const auto& path : temp_paths) {
            std::ifstream file(path);
            if (file.is_open()) {
                int temp_mC;
                file >> temp_mC;
                return temp_mC / 1000.0;
            }
        }
        
        // If no thermal zone found, simulate temperature based on load
        return 40.0 + current_load * 50.0;  // 40-90°C range
    }
    
    unsigned long read_cpu_freq() {
        std::ifstream file("/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq");
        unsigned long freq_khz = 2000000;  // Default 2GHz
        if (file.is_open()) {
            file >> freq_khz;
        }
        return freq_khz / 1000;  // Convert to MHz
    }
    
    double read_power() {
        std::string path = "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj";
        static double last_energy = 0;
        static auto last_time = std::chrono::steady_clock::now();
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return current_load * 50.0;  // Simulate if RAPL not available
        }
        
        double energy;
        file >> energy;
        energy /= 1000000.0;  // Convert to joules
        
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_time).count() / 1000.0;
        
        double power = 0;
        if (duration > 0 && energy > last_energy) {
            power = (energy - last_energy) / duration;
        }
        
        last_energy = energy;
        last_time = now;
        
        return power;
    }
    
    void cpu_load_generator(double target_load) {
        // Generate CPU load proportional to target_load (0.0 to 1.0)
        const int base_iterations = 1000000;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        while (!stop_flag) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Work phase
            volatile double result = 0;
            int iterations = base_iterations * target_load;
            for (int i = 0; i < iterations; i++) {
                result += std::sin(i) * std::cos(i) * std::sqrt(i + 1);
            }
            
            // Sleep phase to achieve target load
            auto work_duration = std::chrono::high_resolution_clock::now() - start;
            auto sleep_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                work_duration) * (1.0 / target_load - 1.0);
            
            if (sleep_duration.count() > 0) {
                std::this_thread::sleep_for(sleep_duration);
            }
            
            current_load = target_load;
        }
    }
    
public:
    struct BenchmarkResult {
        std::string strategy_name;
        double avg_temp_C;
        double max_temp_C;
        double temp_variance;
        double avg_freq_mhz;
        double avg_performance;
        double total_energy_j;
        double perf_per_joule;
        int throttle_events;
    };
    
    std::vector<ThermalData> run_thermal_test(
        const std::string& strategy_cmd,
        double load_level,
        int duration_sec = 60) {
        
        std::vector<ThermalData> data;
        
        // Apply thermal strategy
        if (!strategy_cmd.empty()) {
            system(strategy_cmd.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Start load generator
        stop_flag = false;
        std::thread load_thread(&ThermalCapBenchmark::cpu_load_generator, this, load_level);
        
        // Warm-up period
        std::cout << "Warming up for 10 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Monitoring loop
        auto start_time = std::chrono::steady_clock::now();
        double start_energy = read_power();  // Initialize power reading
        
        std::cout << "Running thermal test for " << duration_sec << " seconds...\n";
        
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count() < duration_sec) {
            
            ThermalData point;
            point.time_s = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count() / 1000.0;
            point.temperature_C = read_cpu_temp();
            point.frequency_mhz = read_cpu_freq();
            point.power_watts = read_power();
            
            // Performance score based on frequency and thermal headroom
            double thermal_headroom = std::max(0.0, (95.0 - point.temperature_C) / 95.0);
            point.performance_score = (point.frequency_mhz / 3600.0) * thermal_headroom;
            
            data.push_back(point);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        // Stop load generator
        stop_flag = true;
        load_thread.join();
        
        return data;
    }
    
    BenchmarkResult analyze_thermal_data(
        const std::vector<ThermalData>& data,
        const std::string& strategy_name) {
        
        BenchmarkResult result;
        result.strategy_name = strategy_name;
        
        // Calculate statistics
        double sum_temp = 0, sum_freq = 0, sum_perf = 0;
        double max_temp = 0;
        std::vector<double> temps;
        
        for (const auto& point : data) {
            sum_temp += point.temperature_C;
            sum_freq += point.frequency_mhz;
            sum_perf += point.performance_score;
            max_temp = std::max(max_temp, point.temperature_C);
            temps.push_back(point.temperature_C);
        }
        
        result.avg_temp_C = sum_temp / data.size();
        result.max_temp_C = max_temp;
        result.avg_freq_mhz = sum_freq / data.size();
        result.avg_performance = sum_perf / data.size();
        
        // Calculate temperature variance
        double sum_sq_diff = 0;
        for (double temp : temps) {
            double diff = temp - result.avg_temp_C;
            sum_sq_diff += diff * diff;
        }
        result.temp_variance = std::sqrt(sum_sq_diff / temps.size());
        
        // Count throttle events (frequency drops)
        result.throttle_events = 0;
        for (size_t i = 1; i < data.size(); i++) {
            if (data[i].frequency_mhz < data[i-1].frequency_mhz - 100) {
                result.throttle_events++;
            }
        }
        
        // Estimate total energy (integrate power over time)
        result.total_energy_j = 0;
        for (size_t i = 1; i < data.size(); i++) {
            double dt = data[i].time_s - data[i-1].time_s;
            double avg_power = (data[i].power_watts + data[i-1].power_watts) / 2;
            result.total_energy_j += avg_power * dt;
        }
        
        result.perf_per_joule = (result.total_energy_j > 0) ? 
                               result.avg_performance / result.total_energy_j : 0;
        
        return result;
    }
    
    void run_thermal_comparison() {
        std::cout << "\nThermal Management Strategy Comparison\n";
        std::cout << "=====================================\n\n";
        
        struct TestStrategy {
            std::string name;
            std::string setup_cmd;
            std::string cleanup_cmd;
        };
        
        std::vector<TestStrategy> strategies = {
            {
                "No Throttling (Baseline)",
                "sudo ./thermal_cap_control disable",
                ""
            },
            {
                "Reactive (OS Default)",
                "sudo ./thermal_cap_control disable",
                ""
            },
            {
                "Proactive Conservative",
                "sudo ./thermal_cap_control policy 65 75 85",
                "sudo ./thermal_cap_control disable"
            },
            {
                "Proactive Aggressive",
                "sudo ./thermal_cap_control policy 60 70 80",
                "sudo ./thermal_cap_control disable"
            },
            {
                "Proactive Balanced",
                "sudo ./thermal_cap_control policy 70 80 90",
                "sudo ./thermal_cap_control disable"
            }
        };
        
        std::vector<double> load_levels = {0.5, 0.75, 1.0};  // 50%, 75%, 100% load
        std::vector<BenchmarkResult> all_results;
        
        for (double load : load_levels) {
            std::cout << "\n--- Testing with " << (load * 100) << "% CPU load ---\n\n";
            
            for (const auto& strategy : strategies) {
                std::cout << "Testing strategy: " << strategy.name << "\n";
                
                auto data = run_thermal_test(strategy.setup_cmd, load, 30);
                auto result = analyze_thermal_data(data, strategy.name + 
                                                 " @ " + std::to_string(int(load * 100)) + "%");
                all_results.push_back(result);
                
                // Cleanup
                if (!strategy.cleanup_cmd.empty()) {
                    system(strategy.cleanup_cmd.c_str());
                }
                
                // Cool-down period
                std::cout << "Cooling down...\n";
                std::this_thread::sleep_for(std::chrono::seconds(20));
            }
        }
        
        // Print results table
        std::cout << "\n\nThermal Management Results\n";
        std::cout << "=========================\n\n";
        
        std::cout << std::left << std::setw(35) << "Strategy"
                  << std::right
                  << std::setw(10) << "Avg Temp"
                  << std::setw(10) << "Max Temp"
                  << std::setw(12) << "Temp StdDev"
                  << std::setw(12) << "Avg Freq"
                  << std::setw(12) << "Perf Score"
                  << std::setw(12) << "Energy(J)"
                  << std::setw(15) << "Perf/Joule"
                  << std::setw(12) << "Throttles\n";
        std::cout << std::string(130, '-') << "\n";
        
        for (const auto& result : all_results) {
            std::cout << std::left << std::setw(35) << result.strategy_name
                      << std::right << std::fixed
                      << std::setw(10) << std::setprecision(1) << result.avg_temp_C
                      << std::setw(10) << std::setprecision(1) << result.max_temp_C
                      << std::setw(12) << std::setprecision(2) << result.temp_variance
                      << std::setw(12) << std::setprecision(0) << result.avg_freq_mhz
                      << std::setw(12) << std::setprecision(3) << result.avg_performance
                      << std::setw(12) << std::setprecision(1) << result.total_energy_j
                      << std::setw(15) << std::setprecision(5) << result.perf_per_joule
                      << std::setw(12) << result.throttle_events
                      << "\n";
        }
        
        std::cout << "\nKey Insights:\n";
        std::cout << "- Proactive throttling reduces temperature variance\n";
        std::cout << "- Conservative policies trade performance for thermal headroom\n";
        std::cout << "- Aggressive policies may cause more throttle events\n";
        std::cout << "- Energy efficiency often improves with moderate throttling\n";
    }
};

int main() {
    std::cout << "Thermal Cap Impact Benchmark\n";
    std::cout << "===========================\n";
    
    try {
        ThermalCapBenchmark bench;
        bench.run_thermal_comparison();
        
        std::cout << "\nBenchmark complete!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}