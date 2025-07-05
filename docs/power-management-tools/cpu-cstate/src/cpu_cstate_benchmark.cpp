/**
 * CPU C-State Benchmark
 * 
 * This benchmark measures the impact of different C-state configurations on:
 * - Wake-up latency
 * - Power consumption during idle
 * - Performance impact on workloads with varying idle patterns
 * - Context switch overhead
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <condition_variable>
#include <mutex>

class CPUCStateBenchmark {
private:
    std::atomic<bool> stop_flag{false};
    std::atomic<int> ready_threads{0};
    std::mutex sync_mutex;
    std::condition_variable sync_cv;
    
    struct LatencyResult {
        double min_us;
        double avg_us;
        double max_us;
        double p50_us;
        double p95_us;
        double p99_us;
    };
    
    double read_cpu_energy() {
        std::string path = "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj";
        std::ifstream file(path);
        if (!file.is_open()) {
            return 0.0;
        }
        double energy;
        file >> energy;
        return energy / 1000000.0; // Convert to joules
    }
    
    std::string get_current_cstate_config() {
        std::string config;
        std::string base = "/sys/devices/system/cpu/cpu0/cpuidle";
        
        for (int i = 0; i < 10; i++) {
            std::string disable_path = base + "/state" + std::to_string(i) + "/disable";
            std::ifstream file(disable_path);
            if (!file.is_open()) break;
            
            std::string disabled;
            file >> disabled;
            config += "C" + std::to_string(i) + ":" + (disabled == "0" ? "on " : "off ");
        }
        return config;
    }
    
public:
    LatencyResult benchmark_wakeup_latency(int iterations = 10000) {
        std::vector<double> latencies;
        latencies.reserve(iterations);
        
        std::cout << "Measuring wake-up latency (" << iterations << " iterations)...\n";
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Sleep for varying durations to trigger different C-states
            int sleep_us = (i % 4 == 0) ? 10 : (i % 4 == 1) ? 100 : (i % 4 == 2) ? 1000 : 10000;
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
            
            auto wake = std::chrono::high_resolution_clock::now();
            
            // Measure time to execute a simple operation after wake
            volatile int dummy = 0;
            for (int j = 0; j < 100; j++) {
                dummy += j;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            
            auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end - wake).count() / 1000.0; // Convert to microseconds
            
            latencies.push_back(latency);
        }
        
        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        
        LatencyResult result;
        result.min_us = latencies.front();
        result.max_us = latencies.back();
        result.avg_us = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        result.p50_us = latencies[latencies.size() * 0.50];
        result.p95_us = latencies[latencies.size() * 0.95];
        result.p99_us = latencies[latencies.size() * 0.99];
        
        return result;
    }
    
    double benchmark_idle_power(int duration_sec = 30) {
        std::cout << "Measuring idle power consumption for " << duration_sec << " seconds...\n";
        
        double energy_start = read_cpu_energy();
        auto time_start = std::chrono::steady_clock::now();
        
        // Let the system idle
        std::this_thread::sleep_for(std::chrono::seconds(duration_sec));
        
        double energy_end = read_cpu_energy();
        auto time_end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            time_end - time_start).count() / 1000.0;
        
        double power = (energy_end > energy_start) ? 
                      (energy_end - energy_start) / duration : 0.0;
        
        return power;
    }
    
    struct WorkloadResult {
        double throughput;  // Operations per second
        double avg_latency_ms;
        double power_watts;
        double energy_per_op_mj;  // Millijoules per operation
    };
    
    WorkloadResult benchmark_intermittent_workload(
        int work_duration_us, 
        int idle_duration_us, 
        int total_duration_sec = 30) {
        
        std::cout << "Running intermittent workload (work: " << work_duration_us 
                  << "us, idle: " << idle_duration_us << "us)...\n";
        
        std::atomic<long> operations{0};
        std::vector<double> latencies;
        std::mutex latency_mutex;
        
        double energy_start = read_cpu_energy();
        auto start = std::chrono::steady_clock::now();
        
        // Worker thread
        std::thread worker([&]() {
            while (!stop_flag) {
                auto work_start = std::chrono::high_resolution_clock::now();
                
                // Simulate work
                volatile double result = 0;
                auto work_end = work_start + std::chrono::microseconds(work_duration_us);
                while (std::chrono::high_resolution_clock::now() < work_end) {
                    for (int i = 0; i < 1000; i++) {
                        result += std::sqrt(i) * std::sin(i);
                    }
                }
                
                auto work_complete = std::chrono::high_resolution_clock::now();
                
                // Record latency
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                    work_complete - work_start).count() / 1000.0; // ms
                
                {
                    std::lock_guard<std::mutex> lock(latency_mutex);
                    latencies.push_back(latency);
                }
                
                operations++;
                
                // Idle period
                std::this_thread::sleep_for(std::chrono::microseconds(idle_duration_us));
            }
        });
        
        // Run for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(total_duration_sec));
        stop_flag = true;
        worker.join();
        stop_flag = false;
        
        double energy_end = read_cpu_energy();
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count() / 1000.0;
        
        WorkloadResult result;
        result.throughput = operations / duration;
        result.avg_latency_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) / 
                               latencies.size();
        result.power_watts = (energy_end > energy_start) ? 
                            (energy_end - energy_start) / duration : 0.0;
        result.energy_per_op_mj = (result.power_watts > 0 && result.throughput > 0) ?
                                 (result.power_watts * 1000) / result.throughput : 0.0;
        
        return result;
    }
    
    void run_cstate_comparison() {
        std::cout << "\nC-State Configuration Comparison\n";
        std::cout << "================================\n\n";
        
        struct TestConfig {
            std::string name;
            std::string command;
        };
        
        std::vector<TestConfig> configs = {
            {"All C-states", ""},  // Default, no change
            {"Max C1 only", "sudo ./cpu_cstate_control max-cstate 1"},
            {"Max C2", "sudo ./cpu_cstate_control max-cstate 2"},
            {"C0/C1 only", "sudo ./cpu_cstate_control max-cstate 0"}
        };
        
        for (const auto& config : configs) {
            std::cout << "\n--- Testing: " << config.name << " ---\n";
            
            if (!config.command.empty()) {
                system(config.command.c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            std::cout << "Current config: " << get_current_cstate_config() << "\n\n";
            
            // 1. Wake-up latency test
            auto latency = benchmark_wakeup_latency(5000);
            std::cout << "Wake-up latency:\n";
            std::cout << "  Min: " << std::fixed << std::setprecision(2) << latency.min_us << " us\n";
            std::cout << "  Avg: " << latency.avg_us << " us\n";
            std::cout << "  P95: " << latency.p95_us << " us\n";
            std::cout << "  P99: " << latency.p99_us << " us\n";
            std::cout << "  Max: " << latency.max_us << " us\n\n";
            
            // 2. Idle power test
            double idle_power = benchmark_idle_power(10);
            std::cout << "Idle power: " << std::fixed << std::setprecision(2) 
                      << idle_power << " W\n\n";
            
            // 3. Workload tests with different idle patterns
            std::cout << "Workload performance:\n";
            
            // Short bursts (likely to use shallow C-states)
            auto short_burst = benchmark_intermittent_workload(100, 100, 10);
            std::cout << "  Short bursts (100us work/100us idle):\n";
            std::cout << "    Throughput: " << std::fixed << std::setprecision(0) 
                      << short_burst.throughput << " ops/s\n";
            std::cout << "    Avg latency: " << std::fixed << std::setprecision(3)
                      << short_burst.avg_latency_ms << " ms\n";
            if (short_burst.energy_per_op_mj > 0) {
                std::cout << "    Energy/op: " << std::fixed << std::setprecision(3)
                          << short_burst.energy_per_op_mj << " mJ\n";
            }
            
            // Medium idle (might reach deeper C-states)
            auto medium_idle = benchmark_intermittent_workload(1000, 5000, 10);
            std::cout << "  Medium idle (1ms work/5ms idle):\n";
            std::cout << "    Throughput: " << std::fixed << std::setprecision(0)
                      << medium_idle.throughput << " ops/s\n";
            std::cout << "    Avg latency: " << std::fixed << std::setprecision(3)
                      << medium_idle.avg_latency_ms << " ms\n";
            if (medium_idle.energy_per_op_mj > 0) {
                std::cout << "    Energy/op: " << std::fixed << std::setprecision(3)
                          << medium_idle.energy_per_op_mj << " mJ\n";
            }
            
            std::cout << std::endl;
        }
        
        // Restore default (all C-states enabled)
        system("sudo ./cpu_cstate_control enable 0");
        system("sudo ./cpu_cstate_control enable 1");
        system("sudo ./cpu_cstate_control enable 2");
        system("sudo ./cpu_cstate_control enable 3");
    }
};

int main() {
    std::cout << "CPU C-State Impact Benchmark\n";
    std::cout << "============================\n";
    
    try {
        CPUCStateBenchmark bench;
        bench.run_cstate_comparison();
        
        std::cout << "\nBenchmark complete!\n";
        std::cout << "\nKey observations:\n";
        std::cout << "- Deeper C-states save more power but have higher wake latency\n";
        std::cout << "- Workloads with short idle periods may not benefit from deep C-states\n";
        std::cout << "- Energy efficiency depends on matching C-state policy to workload pattern\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}