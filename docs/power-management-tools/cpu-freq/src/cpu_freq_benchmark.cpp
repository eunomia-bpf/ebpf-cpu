/**
 * CPU Frequency Benchmark
 * 
 * This benchmark measures the impact of different CPU frequencies on:
 * - Computational performance (FLOPS)
 * - Memory bandwidth
 * - Power efficiency (instructions per joule)
 * - Latency characteristics
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <thread>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <cstring>

class CPUFreqBenchmark {
private:
    static constexpr size_t ARRAY_SIZE = 64 * 1024 * 1024; // 64MB
    static constexpr int ITERATIONS = 100;
    std::vector<double> data_a;
    std::vector<double> data_b;
    std::vector<double> data_c;
    
    unsigned long read_current_freq(int cpu = 0) {
        std::string path = "/sys/devices/system/cpu/cpufreq/policy" + 
                          std::to_string(cpu) + "/scaling_cur_freq";
        std::ifstream file(path);
        unsigned long freq = 0;
        file >> freq;
        return freq;
    }
    
    double read_cpu_energy() {
        // Read from RAPL interface if available
        std::string path = "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj";
        std::ifstream file(path);
        if (!file.is_open()) {
            return 0.0; // RAPL not available
        }
        double energy;
        file >> energy;
        return energy / 1000000.0; // Convert to joules
    }
    
public:
    CPUFreqBenchmark() : data_a(ARRAY_SIZE), data_b(ARRAY_SIZE), data_c(ARRAY_SIZE) {
        // Initialize arrays with random data
        for (size_t i = 0; i < ARRAY_SIZE; i++) {
            data_a[i] = (double)rand() / RAND_MAX;
            data_b[i] = (double)rand() / RAND_MAX;
            data_c[i] = 0.0;
        }
    }
    
    struct BenchmarkResult {
        unsigned long frequency_khz;
        double compute_gflops;
        double memory_bandwidth_gb_s;
        double latency_ns;
        double power_watts;
        double efficiency_gflops_per_watt;
    };
    
    double benchmark_compute() {
        // Perform intensive floating-point operations
        auto start = std::chrono::high_resolution_clock::now();
        
        double sum = 0.0;
        for (int iter = 0; iter < ITERATIONS; iter++) {
            for (size_t i = 0; i < ARRAY_SIZE; i++) {
                // Multiple operations per iteration
                data_c[i] = data_a[i] * data_b[i] + data_c[i];
                data_c[i] = std::sqrt(data_c[i]) + data_a[i];
                data_c[i] = data_c[i] * 0.5 + data_b[i] * 0.5;
                sum += data_c[i];
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        // Calculate GFLOPS (6 operations per iteration)
        double ops = 6.0 * ARRAY_SIZE * ITERATIONS;
        double gflops = ops / duration.count();
        
        // Prevent optimization
        if (sum == 0.0) std::cout << "";
        
        return gflops;
    }
    
    double benchmark_memory_bandwidth() {
        // Memory bandwidth test - copy operation
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int iter = 0; iter < ITERATIONS; iter++) {
            std::memcpy(data_c.data(), data_a.data(), ARRAY_SIZE * sizeof(double));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        // Calculate bandwidth in GB/s
        double bytes = ARRAY_SIZE * sizeof(double) * ITERATIONS * 2; // read + write
        double gb_per_sec = (bytes / 1e9) / (duration.count() / 1e9);
        
        return gb_per_sec;
    }
    
    double benchmark_latency() {
        // Measure single-operation latency
        std::vector<double> latencies;
        
        for (int i = 0; i < 1000; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // Single dependent operations to measure latency
            double val = data_a[i];
            val = std::sqrt(val);
            val = val * val;
            val = std::sqrt(val);
            data_c[i] = val;
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            latencies.push_back(duration.count());
        }
        
        // Return median latency
        std::sort(latencies.begin(), latencies.end());
        return latencies[latencies.size() / 2];
    }
    
    BenchmarkResult run_benchmark() {
        BenchmarkResult result;
        
        // Read current frequency
        result.frequency_khz = read_current_freq();
        
        // Warm up
        benchmark_compute();
        
        // Energy measurement start
        double energy_start = read_cpu_energy();
        auto power_start = std::chrono::steady_clock::now();
        
        // Run benchmarks
        result.compute_gflops = benchmark_compute();
        result.memory_bandwidth_gb_s = benchmark_memory_bandwidth();
        result.latency_ns = benchmark_latency();
        
        // Energy measurement end
        double energy_end = read_cpu_energy();
        auto power_end = std::chrono::steady_clock::now();
        
        auto power_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            power_end - power_start).count() / 1000.0;
        
        if (energy_end > energy_start && power_duration > 0) {
            result.power_watts = (energy_end - energy_start) / power_duration;
            result.efficiency_gflops_per_watt = result.compute_gflops / result.power_watts;
        } else {
            result.power_watts = 0;
            result.efficiency_gflops_per_watt = 0;
        }
        
        return result;
    }
    
    void run_frequency_sweep(const std::vector<unsigned long>& frequencies_khz) {
        std::cout << "\nCPU Frequency Performance Benchmark\n";
        std::cout << "=====================================\n";
        std::cout << std::setw(12) << "Freq(MHz)" 
                  << std::setw(12) << "GFLOPS"
                  << std::setw(15) << "Mem BW(GB/s)"
                  << std::setw(15) << "Latency(ns)"
                  << std::setw(12) << "Power(W)"
                  << std::setw(20) << "GFLOPS/Watt\n";
        std::cout << std::string(86, '-') << std::endl;
        
        std::vector<BenchmarkResult> results;
        
        for (auto freq_khz : frequencies_khz) {
            // Set frequency using the control tool
            std::string cmd = "sudo ./cpu_freq_control set-freq " + 
                             std::to_string(freq_khz / 1000);
            system(cmd.c_str());
            
            // Wait for frequency to stabilize
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Run benchmark
            auto result = run_benchmark();
            results.push_back(result);
            
            // Print results
            std::cout << std::fixed << std::setprecision(0)
                      << std::setw(12) << result.frequency_khz / 1000
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << result.compute_gflops
                      << std::setw(15) << result.memory_bandwidth_gb_s
                      << std::fixed << std::setprecision(1)
                      << std::setw(15) << result.latency_ns
                      << std::fixed << std::setprecision(2)
                      << std::setw(12) << result.power_watts
                      << std::setw(20) << result.efficiency_gflops_per_watt
                      << std::endl;
        }
        
        // Summary
        std::cout << "\nSummary:\n";
        auto max_perf = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.compute_gflops < b.compute_gflops; });
        auto max_eff = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.efficiency_gflops_per_watt < b.efficiency_gflops_per_watt; });
        
        std::cout << "Peak performance: " << max_perf->compute_gflops 
                  << " GFLOPS at " << max_perf->frequency_khz/1000 << " MHz\n";
        if (max_eff->efficiency_gflops_per_watt > 0) {
            std::cout << "Best efficiency: " << max_eff->efficiency_gflops_per_watt 
                      << " GFLOPS/W at " << max_eff->frequency_khz/1000 << " MHz\n";
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "CPU Frequency Impact Benchmark\n";
    std::cout << "==============================\n";
    
    // Default frequency list (modify based on your CPU)
    std::vector<unsigned long> test_frequencies = {
        800000,   // 800 MHz
        1200000,  // 1.2 GHz
        1600000,  // 1.6 GHz
        2000000,  // 2.0 GHz
        2400000,  // 2.4 GHz
        2800000,  // 2.8 GHz
        3200000,  // 3.2 GHz
        3600000   // 3.6 GHz
    };
    
    // Filter to only available frequencies
    std::string freq_file = "/sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies";
    std::ifstream file(freq_file);
    if (file.is_open()) {
        std::vector<unsigned long> available_freqs;
        unsigned long freq;
        while (file >> freq) {
            available_freqs.push_back(freq);
        }
        
        // Use intersection of test and available frequencies
        std::vector<unsigned long> valid_freqs;
        for (auto f : test_frequencies) {
            if (std::find(available_freqs.begin(), available_freqs.end(), f) != available_freqs.end()) {
                valid_freqs.push_back(f);
            }
        }
        test_frequencies = valid_freqs;
    }
    
    if (test_frequencies.empty()) {
        std::cerr << "No valid test frequencies found!\n";
        return 1;
    }
    
    try {
        CPUFreqBenchmark bench;
        bench.run_frequency_sweep(test_frequencies);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}